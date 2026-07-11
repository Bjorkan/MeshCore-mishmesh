#include <mishmesh/applets/settings/SystemInfoPanel.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Locale.h>
#include <mishmesh/core/SystemInfoLocaleStrings.h>
#include <stdio.h>

namespace mishmesh {

static const uint32_t REBUILD_MS = 1000;
static const int ACTION_ROW_H = 12;
static const int ACTIONS_H    = 2 * ACTION_ROW_H;   // exactly two rows

static const SystemInfoTextId RESET_LABELS[2] = {
  SystemInfoTextId::SystemInfoFactoryResetKeepIdentity,
  SystemInfoTextId::SystemInfoFactoryResetFullWipe,
};

static const char* trSystemInfo(SystemInfoTextId id) {
  const uint8_t locale = localeManager().currentIndex();
  const char* translated = generatedSystemInfoLocaleString(locale, id);
  if (translated) return translated;
  const char* fallback = generatedSystemInfoLocaleString(0, id);
  return fallback ? fallback : "";
}

// "142.3K", or "--" when zero (unknown).
static void fmtBytes(char* buf, int n, uint32_t bytes) {
  if (bytes == 0) { snprintf(buf, n, "--"); return; }
  snprintf(buf, n, "%u.%uK", (unsigned)(bytes / 1024),
           (unsigned)((bytes % 1024) * 10 / 1024));
}

int formatSystemStats(const SystemStats& s, char out[][SYSSTATS_LINE_LEN], int maxLines) {
  int n = 0;
  char v[12];
#define SS_EMIT(...) do { if (n < maxLines) snprintf(out[n++], SYSSTATS_LINE_LEN, __VA_ARGS__); } while (0)
  fmtBytes(v, sizeof(v), s.heapFreeBytes);    SS_EMIT(trSystemInfo(SystemInfoTextId::SystemInfoHeapFree), v);
  fmtBytes(v, sizeof(v), s.heapMinFreeBytes); SS_EMIT(trSystemInfo(SystemInfoTextId::SystemInfoHeapMin), v);
  if (s.heapTotalBytes) {
    fmtBytes(v, sizeof(v), s.heapTotalBytes);
    SS_EMIT(trSystemInfo(SystemInfoTextId::SystemInfoHeapTotal), v);
  }
  SS_EMIT(trSystemInfo(SystemInfoTextId::SystemInfoContacts), (unsigned)s.contactsUsed, (unsigned)s.contactsMax);
  if (s.storageTotalKb)
    SS_EMIT(trSystemInfo(SystemInfoTextId::SystemInfoStorage), (unsigned)s.storageUsedKb, (unsigned)s.storageTotalKb);
  else
    SS_EMIT("%s", trSystemInfo(SystemInfoTextId::SystemInfoStorageUnavailable));
  if (s.batteryMv == 0)
    SS_EMIT("%s", trSystemInfo(SystemInfoTextId::SystemInfoBatteryUnavailable));
  else
    SS_EMIT(trSystemInfo(SystemInfoTextId::SystemInfoBattery), (unsigned)(s.batteryMv / 1000),
            (unsigned)((s.batteryMv % 1000) / 10));
  uint32_t mins = s.uptimeSecs / 60;
  SS_EMIT(trSystemInfo(SystemInfoTextId::SystemInfoUptime), (unsigned)(mins / 60), (unsigned)(mins % 60));
  if (s.meshcoreVersion) SS_EMIT("meshcore: %s", s.meshcoreVersion);
  if (s.mishmeshVersion) SS_EMIT("mishmesh: %s", s.mishmeshVersion);
#undef SS_EMIT
  return n;
}

void SystemInfoPanel::rebuild(uint32_t now, bool keepScroll) {
  _lastBuilt = now;
  _built = true;
  _stats.clear(keepScroll);
  SystemStats s;
  if (!(_app && _app->systemStats(s))) {
    _stats.addLine(trSystemInfo(SystemInfoTextId::SystemInfoStatsUnavailable));
    return;
  }
  char lines[SYSSTATS_MAX_LINES][SYSSTATS_LINE_LEN];
  int n = formatSystemStats(s, lines, SYSSTATS_MAX_LINES);
  for (int i = 0; i < n; i++) _stats.addLine(lines[i]);
}

void SystemInfoPanel::begin(AppletContext& ctx) {
  _app = ctx.app;
  _built = false;
  _confirming = false;
  _pendingRow = -1;
  _focus = Focus::Stats;
  _stats.clear();
  _resetLabels[0] = trSystemInfo(RESET_LABELS[0]);
  _resetLabels[1] = trSystemInfo(RESET_LABELS[1]);
  _actions.set(_resetLabels, 2);
  _list.setRowHeight(ACTION_ROW_H);
  _list.setModel(&_actions);
  _list.resetSelection();
  _list.setDrawSelection(false);   // stats region has focus first
  _stats.setFooter(&_list, ACTIONS_H, /*divider=*/true);
}

int SystemInfoPanel::renderBody(Canvas& c, int x, int y, int w, int h) {
  uint32_t now = c.now();
  if (!_built) rebuild(now, false);
  else if (now - _lastBuilt >= REBUILD_MS) rebuild(now, true);

  _list.setDrawSelection(_focus == Focus::Actions);
  _stats.draw(c, x, y, w, h);

  if (_confirming) { _confirm.draw(c, 0, 0, c.width(), c.height()); return 100; }

  bool anim = _stats.needsAnimation() || _list.needsAnimation();
  return anim ? 33 : (int)REBUILD_MS;
}

void SystemInfoPanel::openConfirm(int row) {
  _pendingRow = row;
  _confirm.configure(row == 0
    ? trSystemInfo(SystemInfoTextId::SystemInfoConfirmKeepIdentity)
    : trSystemInfo(SystemInfoTextId::SystemInfoConfirmFullWipe),
    /*defaultSel=*/0);
  _confirming = true;
}

bool SystemInfoPanel::onInput(InputEvent ev) {
  if (_confirming) {
    if (_confirm.onInput(ev)) {
      ConfirmResult r = _confirm.result();
      if (r != ConfirmResult::None) {
        if (r == ConfirmResult::Confirmed && _app) _app->factoryReset(_pendingRow == 0);
        _confirming = false; _pendingRow = -1;
      }
    }
    return true;
  }

  if (_focus == Focus::Stats) {
    if (ev == InputEvent::NavDown && _stats.atBottom()) {
      _focus = Focus::Actions;
      _list.setSelected(0);
      return true;
    }
    return _stats.onInput(ev);
  }

  if (ev == InputEvent::NavUp && _list.selected() == 0) {
    _focus = Focus::Stats;
    return true;
  }
  if (_list.onInput(ev)) return true;
  if (ev == InputEvent::Select) { openConfirm(_list.selected()); return true; }
  return false;
}

SystemInfoPanel& systemInfoSettings() {
  static SystemInfoPanel s;
  return s;
}

}  // namespace mishmesh
