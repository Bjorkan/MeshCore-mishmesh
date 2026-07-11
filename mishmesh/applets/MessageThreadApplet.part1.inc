// mishmesh/applets/MessageThreadApplet.cpp
#include "MessageThreadApplet.h"
#include "MessagePathApplet.h"
#include "ChatNotifyApplet.h"
#include "ChannelShareApplet.h"
#include <mishmesh/applets/KeypadApplet.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Anim.h>
#include <mishmesh/core/RetryEngine.h>   // RetryEngine::MAX_RETRIES (retry label)
#include <mishmesh/core/Locale.h>
#include <mishmesh/core/MessageThreadLocaleStrings.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/core/TimeFormat.h>
#include <mishmesh/widgets/Modal.h>
#include <mishmesh/core/QuickReplyStore.h>
#include <cstdio>
#include <cstring>

namespace mishmesh {

static const char* trMessageThread(MessageThreadTextId id) {
  const uint8_t locale = localeManager().currentIndex();
  const char* translated = generatedMessageThreadLocaleString(locale, id);
  if (translated) return translated;
  const char* fallback = generatedMessageThreadLocaleString(0, id);
  return fallback ? fallback : "";
}

const char* MessageThreadApplet::MsgMenuModel::label(int i) const {
  switch (actionAt(i)) {
    case Reply:  return trMessageThread(MessageThreadTextId::MessageThreadReply);
    case Resend: return trMessageThread(MessageThreadTextId::MessageThreadResend);
    case Delete: return trMessageThread(MessageThreadTextId::MessageThreadDelete);
    default:     return repeats
        ? trMessageThread(MessageThreadTextId::MessageThreadHeardRepeats)
        : trMessageThread(MessageThreadTextId::MessageThreadViewPath);
  }
}

static const int HDR_H   = 13;  // status bar height
static const int GUTTER  = 7;   // left gutter on inbound messages (focus caret + indent)
static const int PAD     = 2;   // bubble inner padding
static const int GAP     = 3;   // vertical gap between message blocks
static const int SB_GUTTER = 4; // width reserved on the right when a scrollbar shows
static const int BTN_H   = 36;  // 2-button action bar at the bottom of the thread

// Word-wraps `text` to `maxW` using fontBody. When draw is true, renders each
// line into `c` starting at (x,y); otherwise only measures. Returns total height
// in px. No allocation: lines are assembled in a fixed stack buffer.
static int wrapText(Canvas& c, int x, int y, int maxW, const char* text,
                    DisplayDriver::Color col, bool draw) {
  const mf_font_s* f = fontBody();
  int adv = c.lineHeight(f);
  if (adv <= 0) adv = 8;
  char line[80];
  int  lineLen = 0;
  int  lines = 0;
  const char* p = text ? text : "";
  while (*p) {
    const char* w0 = p;
    while (*p && *p != ' ' && *p != '\n') p++;
    int wlen = (int)(p - w0);
    if (wlen > (int)sizeof(line) - 1) wlen = (int)sizeof(line) - 1;   // clamp a word to the line buffer

    char cand[80];
    int  clen = 0;
    if (lineLen > 0) {
      memcpy(cand, line, lineLen); clen = lineLen;
      if (clen < (int)sizeof(cand) - 1) cand[clen++] = ' ';
    }
    int room = (int)sizeof(cand) - 1 - clen;        // bytes left for the word (never negative below)
    int copy = wlen < room ? wlen : room;
    if (copy < 0) copy = 0;
    memcpy(cand + clen, w0, copy); clen += copy; cand[clen] = 0;

    if (lineLen > 0 && c.textWidth(f, cand) > maxW) {
      if (draw) { line[lineLen] = 0; c.drawText(f, x, y + lines * adv, line, col); }
      lines++;
      int wl = wlen;
      if (wl > (int)sizeof(line) - 1) wl = (int)sizeof(line) - 1;
      memcpy(line, w0, wl); lineLen = wl; line[lineLen] = 0;
    } else {
      memcpy(line, cand, clen); lineLen = clen; line[lineLen] = 0;
    }
    while (*p == ' ') p++;
    if (*p == '\n') {                       // hard break
      if (draw) { line[lineLen] = 0; c.drawText(f, x, y + lines * adv, line, col); }
      lines++; lineLen = 0; p++;
    }
  }
  if (lineLen > 0) { if (draw) { line[lineLen] = 0; c.drawText(f, x, y + lines * adv, line, col); } lines++; }
  return lines * adv;
}

// Builds the metadata string shown below an outbound bubble.
void MessageThreadApplet::statusLabel(const MessageView& m, char* buf, int cap) {
  if (m.isChannel) {
    if (m.heardCount)
      snprintf(buf, cap, trMessageThread(MessageThreadTextId::MessageThreadHeard),
               (unsigned)m.heardCount);
    else
      snprintf(buf, cap, "%s", trMessageThread(MessageThreadTextId::MessageThreadSent));
  } else if (m.status == ST_DELIVERED) {
    snprintf(buf, cap, trMessageThread(MessageThreadTextId::MessageThreadDelivered),
             m.tripTimeMs / 1000.0);
  } else if (m.status == ST_FAILED) {
    snprintf(buf, cap, "%s", trMessageThread(MessageThreadTextId::MessageThreadFailed));
  } else if (m.retryAttempt > 0) {
    snprintf(buf, cap, trMessageThread(MessageThreadTextId::MessageThreadRetrying),
             (unsigned)m.retryAttempt, (unsigned)RetryEngine::MAX_RETRIES);
  } else {
    snprintf(buf, cap, "%s", trMessageThread(MessageThreadTextId::MessageThreadSent));
  }
}

void MessageThreadApplet::setTarget(const ConvoKey& k, const char* fallbackName) {
  _key = k;
  if (fallbackName) { copyStr(_fallbackName, sizeof(_fallbackName), fallbackName); }
  else _fallbackName[0] = 0;
}

void MessageThreadApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.messages; _app = ctx.app;
  int n = _svc ? _svc->messageCount(_key) : 0;
  _focus = n > 0 ? n - 1 : 0;
  _scrollY = _scrollTarget = 0;
  _animReady = false;   // snap (no slide-in) on the opening frame
  _pinBottom = true;    // first render scrolls to the newest message
  // Guess "has scrollbar" for the first layout. Any chat tall enough to be slow
  // to open overflows, so reserving the gutter up front makes that first measure
  // final - without this the stale state from the previous chat can force a
  // second full remeasure at the flipped width. A short chat that turns out to
  // fit just does one cheap re-layout at full width (few messages).
  _hasScrollbar = true;
  _lastSeq = _svc ? _svc->seq() : 0;
  _prevCount = n;
  _lastMsgTime = 0;        // seed from the newest message so the open frame isn't seen as an arrival
  if (n > 0 && _svc) {
    MessageView mv;
    if (_svc->getMessage(_key, n - 1, mv)) _lastMsgTime = mv.localTime ? mv.localTime : mv.senderTime;
  }
  _unseenBelow = false;    // we open pinned to the newest message
  _menuOpen = false;
  _qrOpen = false;
  _barRow = _composeOnOpen ? 0 : -1;   // open focused on the Write button when requested
  _composeOnOpen = false;              // one-shot

  // Header tab bar: [0] conversation (label tracks _titleBuf), [1] Settings.
  snprintf(_titleBuf, sizeof(_titleBuf), "%s", resolveTitle());
  _tabs.clear();
  _tabs.addTab(_titleBuf, (uint16_t)(_key.type == 1 ? Icon::Users : Icon::User));
  _tabs.addTab(trMessageThread(MessageThreadTextId::MessageThreadSettings), (uint16_t)Icon::Settings);
  _tabs.setSelected(0);
  _chatMenu.setTarget(_key);
  _chatMenu.reset();
  refreshRegion();

  if (_svc) _svc->setActiveConvo(_key);
}

void MessageThreadApplet::onForeground() {
  snprintf(_titleBuf, sizeof(_titleBuf), "%s", resolveTitle());
  if (_svc) _svc->setActiveConvo(_key);
  refreshRegion();
}
void MessageThreadApplet::onBackground() { if (_svc) _svc->clearActiveConvo(); }
void MessageThreadApplet::onStop() {
  if (!_svc) return;
  // Leaving the chat with a message still below the fold (chevron showing) means it
  // was never actually read - being the active convo had suppressed its unread
  // badge, so restore one on the way out.
  if (_unseenBelow) _svc->markUnread(_key);
  _svc->clearActiveConvo();
}

const char* MessageThreadApplet::resolveTitle() const {
  ConvoView cv;
  for (int i = 0; _svc && i < _svc->convoCount(); i++) {
    if (_svc->getConvo(i, cv) && cv.key.equals(_key)) return cv.name;
  }
  return _fallbackName;
}

int MessageThreadApplet::blockHeight(Canvas& body, const MessageView& m) const {
  int adv = body.lineHeight(fontBody());    if (adv <= 0) adv = 8;
  int cap = body.lineHeight(fontCaption()); if (cap <= 0) cap = 6;   // recessive metadata row
  int cw = _contentW;
  if (m.outbound) {
    int bubbleW = (cw * 3) / 4;
    int bodyH = wrapText(body, 0, 0, bubbleW - 2 * PAD, m.text, DisplayDriver::DARK, false);
    if (bodyH < adv) bodyH = adv;
    return (bodyH + 2 * PAD) + cap + GAP;          // bubble (body font) + caption status + gap
  }
  int bodyH = wrapText(body, 0, 0, cw - GUTTER - PAD, m.text, DisplayDriver::LIGHT, false);
  if (bodyH < adv) bodyH = adv;
  return cap + bodyH + GAP;                         // caption sender/divider row + body + gap
}

// Fill the per-message height cache and _contentH. Only touches flash (getMessage)
// when the cache is stale: a full remeasure on chat/width change, or a single
// entry when exactly one message was appended. A pure re-render is a cache hit.
void MessageThreadApplet::ensureHeights(Canvas& body, int n) {
  if (n > LAY_MAX) n = LAY_MAX;
  // _lastMsgTime is this chat's newest-message time, refreshed earlier this frame.
  const bool sameShape = _layValid && _layKey.equals(_key) && _layW == _contentW;
  if (sameShape && _layNewest == _lastMsgTime && _layN == n) return;   // this chat unchanged
  if (sameShape && n == _layN + 1) {                       // one message appended
    MessageView m;
    _blkH[n - 1] = (_svc && _svc->getMessage(_key, n - 1, m)) ? (uint16_t)blockHeight(body, m) : 0;
  } else if (_svc) {                                        // full (re)measure - ONE sequential flash pass
    for (int i = 0; i < n; i++) _blkH[i] = 0;
    struct Ctx { MessageThreadApplet* self; Canvas* body; } cx{ this, &body };
    _svc->forEachMessage(_key, [](void* p, int idx, const MessageView& m) {
      Ctx* cx = static_cast<Ctx*>(p);
      if (idx >= 0 && idx < PER_CHAT_CAP) cx->self->_blkH[idx] = (uint16_t)cx->self->blockHeight(*cx->body, m);
    }, &cx);
  }
  int top = 0;
  for (int i = 0; i < n; i++) top += _blkH[i];
  _contentH = top + BTN_H;                                 // action bar always trails the messages
  _layValid = true; _layKey = _key; _layW = _contentW; _layN = n; _layNewest = _lastMsgTime;
}

// Locate the focused message's top/bottom from cached heights (no flash). Cheap
// enough to run every frame; _focus changes on nav without touching the message set.
void MessageThreadApplet::computeFocusSpan(int n) {
  if (n > LAY_MAX) n = LAY_MAX;
  int top = 0;
  _focusTop = _focusBot = 0;
  for (int i = 0; i < n; i++) {
    if (i == _focus && _barRow < 0) { _focusTop = top; _focusBot = top + _blkH[i]; }
    top += _blkH[i];
  }
  if (_barRow >= 0) { _focusTop = top; _focusBot = top + BTN_H; }  // whole bar visible
}

void MessageThreadApplet::layoutFocus(Canvas& body, int n) {
  ensureHeights(body, n);
  computeFocusSpan(n);
}

// Pulls the committed scroll target just far enough to keep the focused message
// visible (the eased _scrollY then glides toward it).
void MessageThreadApplet::adjustScroll() {
  int vp = _bodyH;
  if (_contentH <= vp) { _scrollTarget = 0; return; }
  if (_focusBot - _focusTop >= vp) {               // focused message taller than viewport
    if (_scrollTarget < _focusTop)      _scrollTarget = _focusTop;
    if (_scrollTarget > _focusBot - vp) _scrollTarget = _focusBot - vp;
  } else {
    if (_focusTop < _scrollTarget)       _scrollTarget = _focusTop;
    if (_focusBot > _scrollTarget + vp)  _scrollTarget = _focusBot - vp;
  }
  if (_scrollTarget < 0) _scrollTarget = 0;
  if (_scrollTarget > _contentH - vp) _scrollTarget = _contentH - vp;
}

void MessageThreadApplet::msgStamp(const MessageView& m, char* out, uint16_t cap) const {
  formatStamp(out, cap, m.localTime ? m.localTime : m.senderTime,
              _app ? _app->epochSeconds() : 0,
              _app ? _app->tzOffsetMinutes() : 0,
              _app ? _app->timeFormat12h() : false);
}

