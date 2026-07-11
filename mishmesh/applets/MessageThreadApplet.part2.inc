void MessageThreadApplet::drawMessage(Canvas& body, const MessageView& m, int top, int blockH, bool focused) const {
  int adv = body.lineHeight(fontBody());    if (adv <= 0) adv = 8;
  int cap = body.lineHeight(fontCaption()); if (cap <= 0) cap = 6;
  int bh = blockH;   // cached height; no re-measure here
  // Clip the whole block to its own region (negative offset keeps mapping).
  Canvas blk = body.region(0, top - _scrollY, body.width(), bh);
  int cw = _contentW;

  if (m.outbound) {
    int bubbleW = (cw * 3) / 4;
    int bx = cw - bubbleW;
    // blockHeight() defines an outbound block as (bodyH+2*PAD) + cap + GAP, so
    // the bubble height is the cached height minus the caption row and gap - no
    // need to re-wrap the text just to size the bubble.
    int bubbleH = bh - cap - GAP;
    if (bubbleH < adv + 2 * PAD) bubbleH = adv + 2 * PAD;
    blk.fillRect(bx, 0, bubbleW, bubbleH, DisplayDriver::LIGHT);
    wrapText(blk, bx + PAD, PAD, bubbleW - 2 * PAD, m.text, DisplayDriver::DARK, true);
    char st[28]; statusLabel(m, st, sizeof(st));
    char ts[20]; msgStamp(m, ts, sizeof(ts));
    if (ts[0]) blk.drawText(fontCaption(), bx, bubbleH, ts, DisplayDriver::LIGHT, TextAlign::Left);
    blk.drawText(fontCaption(), cw, bubbleH, st, DisplayDriver::LIGHT, TextAlign::Right);
    if (focused) blk.drawText(fontBody(), bx - 6, (bubbleH - adv) / 2, ">", DisplayDriver::LIGHT);
    return;
  }

  // Inbound: no box. A caption-size sender label (channel/room post) or a short
  // divider marks the start. Room posts set senderName with isChannel=false.
  if (m.senderName && m.senderName[0]) {
    blk.drawText(fontCaption(), GUTTER, 0, m.senderName, DisplayDriver::LIGHT);
    int nw = blk.textWidth(fontCaption(), m.senderName);
    blk.fillRect(GUTTER, cap - 1, nw, 1, DisplayDriver::LIGHT);   // underline = it's a label
  } else {
    blk.fillRect(GUTTER, cap / 2, 16, 1, DisplayDriver::LIGHT);   // short divider
  }
  char ts[20]; msgStamp(m, ts, sizeof(ts));
  if (ts[0]) blk.drawText(fontCaption(), cw, 0, ts, DisplayDriver::LIGHT, TextAlign::Right);
  wrapText(blk, GUTTER, cap, cw - GUTTER - PAD, m.text, DisplayDriver::LIGHT, true);
  if (focused) blk.drawText(fontBody(), 0, cap, ">", DisplayDriver::LIGHT);
}

// Draws the two stacked Write/Quick buttons into a BTN_H-tall region; the
// focused button (per _barRow) renders filled.
void MessageThreadApplet::drawActionBar(Canvas& bar) {
  const int MX = 2, MY = 1, GAP = 2;             // outer margin + inter-button gap
  int bw = bar.width() - 2 * MX;
  int bh = (BTN_H - 2 * MY - GAP) / 2;
  _btn.set(trMessageThread(MessageThreadTextId::MessageThreadWriteAMessage), (uint16_t)Icon::Feather);
  _btn.setFocused(_barRow == 0);
  _btn.draw(bar, MX, MY, bw, bh);
  _btn.set(trMessageThread(MessageThreadTextId::MessageThreadQuickReplies), (uint16_t)Icon::Zap);
  _btn.setFocused(_barRow == 1);
  _btn.draw(bar, MX, MY + bh + GAP, bw, bh);
}

int MessageThreadApplet::onRender(Canvas& c) {
  int n = _svc ? _svc->messageCount(_key) : 0;

  // Detect a genuinely new message by the newest message's timestamp, not by count.
  // At the per-chat cap an arrival evicts the oldest, so messageCount() stays flat
  // and a count test would miss it entirely. localTime is our receive clock; fall
  // back to senderTime when the RTC is unset. The count test is kept as a secondary
  // signal for the (common) below-cap case so same-second arrivals still register.
  uint32_t newestTime = 0;
  if (n > 0 && _svc) {
    MessageView mv;
    if (_svc->getMessage(_key, n - 1, mv)) newestTime = mv.localTime ? mv.localTime : mv.senderTime;
  }
  bool newArrived = (n > _prevCount) || (newestTime > _lastMsgTime);

  if (newArrived) {
    // The caret never follows an arrival - the user's reading position stays put.
    // But if the cap evicted the oldest to make room, every index shifted down, so
    // step the caret down by however many were dropped to keep it on the SAME
    // message instead of letting it jump to the next one. (Assumes one new message
    // per step, which holds in practice - the UI repaints between arrivals.)
    int evicted = (_prevCount + 1) - n;
    if (evicted > 0) { _focus -= evicted; if (_focus < 0) _focus = 0; }
  }
  _prevCount = n;
  _lastMsgTime = newestTime;
  if (_svc && _svc->seq() != _lastSeq) {
    _lastSeq = _svc->seq();
    if (_focus >= n) _focus = n > 0 ? n - 1 : 0;   // clamp after deletions
  }

  // Header: tab bar ([0] conversation, [1] Settings) with a floating battery.
  _tabs.setBattery(_app ? _app->batteryMillivolts() : 0);
  _tabs.draw(c, 0, 0, c.width(), HDR_H);

  Canvas body = c.region(0, HDR_H, c.width(), c.height() - HDR_H);
  _bodyH = body.height();

  // Settings tab: the shared chat menu inline (replaces the message view).
  if (_tabs.selected() == 1) {
    body.fillRect(0, 0, body.width(), body.height(), DisplayDriver::DARK);
    _chatMenu.draw(body, 4, 2, body.width() - 8, body.height() - 4);
    if (_chatMenu.modalActive())   // full-screen guard over header + menu
      _chatMenu.drawModal(c, 0, 0, c.width(), c.height());
    return _chatMenu.needsAnimation() ? ListMenu::TICK_MS : 250;
  }

  // Empty thread: the hint centered in the space above a bottom-pinned action
  // bar (no scroll machinery - there's nothing to lay out).
  if (n == 0) {
    int barTop = body.height() - BTN_H; if (barTop < 0) barTop = 0;
    int hintY = (barTop - body.fontHeight(fontBody())) / 2; if (hintY < 0) hintY = 0;
    body.drawText(fontBody(), body.width() / 2, hintY,
                  trMessageThread(MessageThreadTextId::MessageThreadNoMessages),
                  DisplayDriver::LIGHT, TextAlign::Center);
    Canvas barC = body.region(0, barTop, body.width(), BTN_H);
    drawActionBar(barC);
    // Quick replies open from the empty-thread action bar, so the modal must
    // still paint here - the main modal draw below is past this early return.
    if (_qrOpen) {
      Canvas box = drawModalChrome(c);
      _menu.draw(box, 2, 2, box.width() - 4, box.height() - 4);
      return _menu.needsAnimation() ? ListMenu::TICK_MS : 250;
    }
    return 500;
  }

  // Reserve the scrollbar gutter from the STICKY overflow state so _contentW -
  // and therefore the height cache - stays stable frame-to-frame. Only when the
  // overflow state actually flips do we re-lay out at the new width (one rebuild),
  // instead of measuring at full then narrow width on every single frame.
  _contentW = body.width() - (_hasScrollbar ? SB_GUTTER : 0);
  layoutFocus(body, n);
  bool scrollbar = _contentH > _bodyH;
  if (scrollbar != _hasScrollbar) {
    _hasScrollbar = scrollbar;
    _contentW = body.width() - (scrollbar ? SB_GUTTER : 0);
    layoutFocus(body, n);
  }

  if (_pinBottom) {
    _scrollTarget = (_contentH > _bodyH) ? _contentH - _bodyH : 0;
    _animReady = false;          // snap to the bottom on open, no slide-in
    _pinBottom = false;
  }
  adjustScroll();

  // New-message chevron, decided from actual visibility (the caret is never moved).
  // The newest message's bottom sits BTN_H above the content end - the action bar
  // always trails it - so test against that, not the absolute bottom. Raise the
  // marker only when a message *arrived* below the fold; clear it the moment the
  // newest message is back in view. Plain scrolling through history never raises it.
  {
    int newestBottom = _contentH - BTN_H;
    bool newestVisible = (n == 0) || (newestBottom <= _scrollTarget + _bodyH);
    if (newArrived && !newestVisible) _unseenBelow = true;
    if (newestVisible)                _unseenBelow = false;
  }

  // Ease the current scroll toward the target (same feel as ListMenu/ScrollText).
  if (!_animReady) { _scrollY = _scrollTarget; _animReady = true; }
  else {
    int step = _bodyH / 3; if (step < 4) step = 4;
    _scrollY = approach(_scrollY, _scrollTarget, step);
  }
  bool animating = (_scrollY != _scrollTarget);

  // Draw only the messages intersecting the viewport. Skip fully-above rows using
  // cached heights (no getMessage/flash), then decode+draw just the visible band -
  // so per-frame flash reads scale with what's on screen, not with chat length.
  if (n > LAY_MAX) n = LAY_MAX;
  int top = 0;
  int i = 0;
  for (; i < n; i++) {                          // skip everything above the viewport
    if (top + (int)_blkH[i] - _scrollY > 0) break;
    top += _blkH[i];
  }
  for (; i < n; i++) {
    int y = top - _scrollY;
    if (y >= _bodyH) break;                     // first row past the bottom -> done
    MessageView m;
    if (_svc->getMessage(_key, i, m)) drawMessage(body, m, top, _blkH[i], i == _focus);
    top += _blkH[i];
  }

  // Action bar: two stacked bordered buttons as an inline trailing block below
  // the messages. Drawn here (this applet owns the _barRow focus state machine).
  {
    int by = (_contentH - BTN_H) - _scrollY;
    if (by + BTN_H > 0 && by < _bodyH) {
      Canvas barC = body.region(0, by, _contentW, BTN_H);
      drawActionBar(barC);
    }
  }

  // Scrollbar thumb on the right edge (content already cleared SB_GUTTER for it).
  if (scrollbar) {
    body.drawScrollbarThumb(body.width() - 2, _bodyH, _contentH, _scrollY);
  }

  // New-message chevron: a small downward marker pinned to the bottom-centre of
  // the viewport while there's unread content below the fold. Slow blink to draw
  // the eye; clears itself (above) once the user scrolls back down.
  if (_unseenBelow) {
    const int bw = 13, bh = 8;
    int bx = (body.width() - bw) / 2, by = _bodyH - bh;
    body.fillRect(bx, by, bw, bh, DisplayDriver::DARK);
    body.drawRoundRect(bx, by, bw, bh, DisplayDriver::LIGHT);
    if (((body.now() / 500) & 1) == 0) {           // blink at ~1 Hz
      int cx = bx + bw / 2, ty = by + 2;
      body.fillRect(cx - 2, ty,     5, 1, DisplayDriver::LIGHT);
      body.fillRect(cx - 1, ty + 1, 3, 1, DisplayDriver::LIGHT);
      body.fillRect(cx,     ty + 2, 1, 1, DisplayDriver::LIGHT);
    }
  }

  // Per-message action menu: a bare box over the live conversation.
  if (_menuOpen) {
    Canvas box = drawModalChrome(c);
    _menu.draw(box, 2, 2, box.width() - 4, box.height() - 4);
    return _menu.needsAnimation() ? ListMenu::TICK_MS : 250;
  }
  if (_qrOpen) {
    Canvas box = drawModalChrome(c);
    _menu.draw(box, 2, 2, box.width() - 4, box.height() - 4);
    return _menu.needsAnimation() ? ListMenu::TICK_MS : 250;
  }
  return animating ? ListMenu::TICK_MS : 500;
}

