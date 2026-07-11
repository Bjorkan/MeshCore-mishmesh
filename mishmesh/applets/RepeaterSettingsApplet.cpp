// mishmesh/applets/RepeaterSettingsApplet.cpp
#include <mishmesh/applets/RepeaterSettingsApplet.h>
#include <mishmesh/applets/AppletChrome.h>
#include <mishmesh/applets/RepeaterSettingsPanel.h>
#include <mishmesh/applets/RepeaterActionsPanel.h>
#include <mishmesh/applets/RepeaterRadioPanel.h>
#include <mishmesh/applets/RepeaterTelemetryApplet.h>
#include <mishmesh/applets/RepeaterNeighborsApplet.h>
#include <mishmesh/applets/RepeaterRegionsApplet.h>
#include <mishmesh/applets/RepeaterAclApplet.h>
#include <mishmesh/applets/RepeaterIdentityApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Locale.h>
#include <mishmesh/core/RepeaterSettingsLocaleStrings.h>
#include <mishmesh/text/Fonts.h>
#include <string.h>

namespace mishmesh {

using SF = SettingFieldDef;

static const char* trRepeaterSettings(RepeaterSettingsTextId id) {
  const uint8_t locale = localeManager().currentIndex();
  const char* translated = generatedRepeaterSettingsLocaleString(locale, id);
  if (translated) return translated;
  const char* fallback = generatedRepeaterSettingsLocaleString(0, id);
  return fallback ? fallback : "";
}

// P2a field tables
static SF PUBLIC_INFO[] = {
  {nullptr, "get name",       "set name", SF::Text, 0, 0},
  {nullptr, "get public.key", nullptr,    SF::ReadOnly, 0, 0, nullptr, true},
  {nullptr, "get role",       nullptr,    SF::ReadOnly, 0, 0},
};
static SF OWNER[] = {
  {nullptr, "get owner.info", "set owner.info", SF::Text, 0, 0},
};
static SF POSITION[] = {
  {nullptr, "get lat", "set lat", SF::Float, 0, 0},
  {nullptr, "get lon", "set lon", SF::Float, 0, 0},
};
static SF ADVERT[] = {
  {nullptr, "get advert.interval",       "set advert.interval",       SF::Number, 60, 240},
  {nullptr, "get flood.advert.interval", "set flood.advert.interval", SF::Number, 3, 168},
};
static SF REPEAT[] = {
  {nullptr, "get repeat", "set repeat", SF::Toggle, 0, 0},
};
static SF PASSWORDS[] = {
  {nullptr, nullptr, "password",           SF::Text, 0, 0, "password now:"},
  {nullptr, nullptr, "set guest.password", SF::Text, 0, 0},
};
static SF VERSION[] = {
  {nullptr, "ver", nullptr, SF::ReadOnly, 0, 0},
};

// Catalog: a panel (defs+n+title), the actions applet, or a coming-soon placeholder.
struct Entry { const char* label; const SF* defs; int n; const char* title; uint8_t kind; };
enum { K_PANEL = 0, K_ACTIONS = 1, K_SOON = 2, K_RADIO = 3, K_TELEM = 4, K_NEIGH = 5, K_REGIONS = 6, K_ACL = 7, K_IDENTITY = 8 };
static Entry CATALOG[] = {
  {nullptr, PUBLIC_INFO, 3, nullptr, K_PANEL},
  {nullptr, nullptr,     0, nullptr, K_RADIO},
  {nullptr, OWNER,       1, nullptr, K_PANEL},
  {nullptr, POSITION,    2, nullptr, K_PANEL},
  {nullptr, ADVERT,      2, nullptr, K_PANEL},
  {nullptr, REPEAT,      1, nullptr, K_PANEL},
  {nullptr, PASSWORDS,   2, nullptr, K_PANEL},
  {nullptr, nullptr,     0, nullptr, K_ACL},
  {nullptr, nullptr,     0, nullptr, K_IDENTITY},
  {nullptr, nullptr,     0, nullptr, K_REGIONS},
  {nullptr, nullptr,     0, nullptr, K_TELEM},
  {nullptr, nullptr,     0, nullptr, K_NEIGH},
  {nullptr, VERSION,     1, nullptr, K_PANEL},
  {nullptr, nullptr,     0, nullptr, K_ACTIONS},
};
static const int CATALOG_N = (int)(sizeof(CATALOG) / sizeof(CATALOG[0]));

static void applyRepeaterSettingsLocale() {
  PUBLIC_INFO[0].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsName);
  PUBLIC_INFO[1].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsPublicKey);
  PUBLIC_INFO[2].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsRole);
  OWNER[0].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsOwnerInfo);
  POSITION[0].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsLatitude);
  POSITION[1].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsLongitude);
  ADVERT[0].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsZeroHopMin);
  ADVERT[1].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsFloodHr);
  REPEAT[0].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsRepeatMode);
  PASSWORDS[0].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsAdminPassword);
  PASSWORDS[1].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsGuestPassword);
  VERSION[0].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsVersion);

  CATALOG[0].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsPublicInfo);
  CATALOG[0].title = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsPublicInfo);
  CATALOG[1].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsRadio);
  CATALOG[2].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsOwnerInfo);
  CATALOG[2].title = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsOwner);
  CATALOG[3].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsPosition);
  CATALOG[3].title = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsPosition);
  CATALOG[4].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsAdvert);
  CATALOG[4].title = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsAdvert);
  CATALOG[5].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsRepeatMode);
  CATALOG[5].title = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsRepeat);
  CATALOG[6].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsPasswords);
  CATALOG[6].title = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsPasswords);
  CATALOG[7].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsAccessControl);
  CATALOG[8].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsChangeIdentity);
  CATALOG[9].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsRegions);
  CATALOG[10].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsTelemetry);
  CATALOG[11].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsNeighbors);
  CATALOG[12].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsVersion);
  CATALOG[12].title = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsVersion);
  CATALOG[13].label = trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsActions);
}

namespace {
struct SettingsHubModel : ListModel {
  int count() const override { return CATALOG_N; }
  const char* label(int i) const override { return CATALOG[i].label; }
};
static SettingsHubModel s_hubModel;
}  // namespace

void RepeaterSettingsApplet::setTarget(const uint8_t* pubKey, const char* name) {
  setTargetFields(_pub, _name, sizeof(_name), pubKey, name);
}

void RepeaterSettingsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  applyRepeaterSettingsLocale();
  _menu.setModel(&s_hubModel);
  _menu.resetSelection();
}

// Embedded-mode activation: update host reference. Menu state (current selection)
// is preserved across tab switches; onStart resets it on fresh hub open.
void RepeaterSettingsApplet::onShow(AppletContext& ctx) {
  _host = ctx.host;
  applyRepeaterSettingsLocale();
}

int RepeaterSettingsApplet::onRender(Canvas& c) {
  return renderBody(c, 0, 0, c.width(), c.height());
}

// Embedded-mode render: draws the catalog list into the given sub-region.
int RepeaterSettingsApplet::renderBody(Canvas& c, int x, int y, int w, int h) {
  _menu.draw(c, x, y, w, h);
  return _menu.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool RepeaterSettingsApplet::onInput(InputEvent ev) {
  if (_menu.onInput(ev)) return true;
  if (ev == InputEvent::Select) {
    const Entry& e = CATALOG[_menu.selected()];
    if (e.kind == K_PANEL) {
      repeaterSettingsPanel().setTarget(_pub, _name);
      repeaterSettingsPanel().setModel(e.defs, e.n, e.title);
      if (_host) _host->push(&repeaterSettingsPanel());
    } else if (e.kind == K_ACTIONS) {
      repeaterActionsPanel().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterActionsPanel());
    } else if (e.kind == K_RADIO) {
      repeaterRadioPanel().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterRadioPanel());
    } else if (e.kind == K_TELEM) {
      repeaterTelemetryApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterTelemetryApplet());
    } else if (e.kind == K_NEIGH) {
      repeaterNeighborsApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterNeighborsApplet());
    } else if (e.kind == K_REGIONS) {
      repeaterRegionsApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterRegionsApplet());
    } else if (e.kind == K_ACL) {
      repeaterAclApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterAclApplet());
    } else if (e.kind == K_IDENTITY) {
      repeaterIdentityApplet().setTarget(_pub, _name);
      if (_host) _host->push(&repeaterIdentityApplet());
    } else {
      if (_host) _host->postToast(
          trRepeaterSettings(RepeaterSettingsTextId::RepeaterSettingsComingSoon));
    }
    return true;
  }
  return false;   // Back bubbles to the management hub
}

RepeaterSettingsApplet& repeaterSettingsApplet() {
  static RepeaterSettingsApplet s_settings;
  return s_settings;
}

}  // namespace mishmesh
