#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Locale.h>
#include <string.h>

namespace mishmesh {

static AppletRegistration* s_head = nullptr;

void registerApplet(AppletRegistration* reg) {
  if (reg == nullptr) return;
  reg->next = s_head;
  s_head = reg;
}

AppletRegistration* registeredApplets() {
  return s_head;
}

const char* appletDisplayLabel(const AppletRegistration* reg) {
  if (!reg || !reg->label) return "";

  struct LabelMap {
    const char* stableLabel;
    TextId textId;
  };
  static const LabelMap LABELS[] = {
    {"Messages", TextId::AppMessages},
    {"Contacts", TextId::AppContacts},
    {"Advert", TextId::AppAdvert},
    {"Clock", TextId::AppClock},
    {"Airtime", TextId::AppAirtime},
    {"Settings", TextId::AppSettings},
    {"About", TextId::AppAbout},
  };

  for (unsigned i = 0; i < sizeof(LABELS) / sizeof(LABELS[0]); ++i) {
    if (strcmp(reg->label, LABELS[i].stableLabel) == 0)
      return tr(LABELS[i].textId);
  }
  return reg->label;
}

void resetRegistry() {
  s_head = nullptr;
}

}  // namespace mishmesh
