#include <mishmesh/applets/AdvertApplet.h>
#include <mishmesh/applets/settings/AdvertSettingsPanel.h>
#include <mishmesh/applets/ContactDetailApplet.h>   // contactDetailApplet()
#include <mishmesh/applets/DiscoverDetailApplet.h>  // discoverDetailApplet()
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Locale.h>
#include <mishmesh/core/ContactFormat.h>             // kindIcon / contactLabel / contactFormatAge
#include <mishmesh/text/Fonts.h>
#include <stdio.h>

namespace mishmesh {

static const TextId kSendLabel[2] = { TextId::AdvertSendZeroHop, TextId::AdvertSendFloodRouted };

const char* AdvertApplet::SendModel::label(int index) const {
  return (index >= 0 && index < 2) ? tr(kSendLabel[index]) : "";
}
uint16_t AdvertApplet::SendModel::icon(int index) const {
  return (uint16_t)(index == 1 ? Icon::Radio : Icon::Wifi);   // 0 = local/zero-hop, 1 = network/flood
}

int AdvertApplet::RecentModel::count() const { return _svc ? _svc->countRecentAdverts() : 0; }

const char* AdvertApplet::RecentModel::label(int index) const {
  static ContactView v;
  static char buf[44];
  if (!_svc || !_svc->getRecentAdvert(index, v)) return "";
  return contactLabel(v, buf, sizeof(buf));
}
uint16_t AdvertApplet::RecentModel::icon(int index) const {
  static ContactView v;
  if (!_svc || !_svc->getRecentAdvert(index, v)) return 0;
  return kindIcon((ContactKind)v.type);
}
const char* AdvertApplet::RecentModel::value(int index) const {
  static ContactView v;
  static char buf[12];
  if (!_svc || !_app || !_svc->getRecentAdvert(index, v)) return nullptr;
  uint32_t now = _app->epochSeconds();
  if (now == 0 || v.heardAt == 0 || now < v.heardAt) return nullptr;
  contactFormatAge(now - v.heardAt, buf, sizeof(buf));
  return buf;
}

void AdvertApplet::onStart(AppletContext& ctx) {
  _app = ctx.app; _svc = ctx.contacts; _host = ctx.host;
  _recent.bind(_svc, _app);
  advertSettings().begin(ctx);
  _tabs.clear();
  _tabs.addTab(tr(TextId::AdvertTabAdvert), (uint16_t)Icon::Radio);
  _tabs.addTab(tr(TextId::AdvertTabRecent), (uint16_t)Icon::Search);
  _tabs.addTab(tr(TextId::AdvertTabSettings), (uint16_t)Icon::Settings);
  _list.setRowHeight(14);
  syncListToTab();
  _list.resetSelection();
}

void AdvertApplet::syncListToTab() {
  switch (_tabs.selected()) {
    case 0:  _list.setModel(&_send);     _list.setEmptyText(nullptr);            break;
    case 1:  _list.setModel(&_recent);   _list.setEmptyText(tr(TextId::AdvertEmptyNoAdverts));   break;
    default: break;   // Settings tab: rendered by advertSettings(), not _list
  }
}

int AdvertApplet::onRender(Canvas& c) {
  int w = c.width(), h = c.height();
  int barH = 13;
  _tabs.setBattery(_app ? _app->batteryMillivolts() : 0);
  _tabs.draw(c, 0, 0, w, barH);
  int bodyY = barH + 1;
  if (settingsTab()) return advertSettings().renderBody(c, 0, bodyY, w, h - bodyY);
  _list.draw(c, 0, bodyY, w, h - bodyY);
  return _list.needsAnimation() ? ListMenu::TICK_MS : 1000;
}

bool AdvertApplet::onInput(InputEvent ev) {
  if (settingsTab()) {
    if (advertSettings().modalActive()) return advertSettings().onInput(ev);
    if (_tabs.onInput(ev)) { syncListToTab(); return true; }   // NavLeft/Right leave settings
    if (advertSettings().onInput(ev)) return true;
    return false;   // Back bubbles to the host
  }
  if (_tabs.onInput(ev)) { syncListToTab(); return true; }
  if (_list.onInput(ev)) return true;

  if (ev == InputEvent::Select) {
    if (_tabs.selected() == 0) {                             // Advert (send) tab
      bool flood = (_list.selected() == 1);
      bool ok = _app && _app->sendAdvert(flood);
      if (_host) {
        _host->postToast(ok ? (flood ? tr(TextId::AdvertToastFloodSent)
                                      : tr(TextId::AdvertToastZeroHopSent))
                            : tr(TextId::AdvertToastFailed));
      }
      return true;
    }
    // Recent tab: route to the contact detail (if a contact) or discover detail.
    if (_svc) {
      ContactView v;
      if (_svc->getRecentAdvert(_list.selected(), v)) {
        if (_svc->isContact(v.pubKey)) {
          contactDetailApplet().setTarget(v.pubKey);
          if (_host) _host->push(&contactDetailApplet());
        } else {
          discoverDetailApplet().setTarget(v);
          if (_host) _host->push(&discoverDetailApplet());
        }
      }
    }
    return true;
  }
  return false;
}

static AdvertApplet s_advert;
MISHMESH_REGISTER_APPLET_ICON(&s_advert, ::mishmesh::Placement::AppMenu,
                              "Advert", 3, (uint16_t)::mishmesh::Icon::Radio);

}  // namespace mishmesh
