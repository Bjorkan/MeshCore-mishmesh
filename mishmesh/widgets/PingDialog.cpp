#include <mishmesh/widgets/PingDialog.h>
#include <mishmesh/widgets/Modal.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/Locale.h>
#include <mishmesh/core/PingDialogLocaleStrings.h>

namespace mishmesh {

static const char* trPingDialog(PingDialogTextId id) {
  const uint8_t locale = localeManager().currentIndex();
  const char* translated = generatedPingDialogLocaleString(locale, id);
  if (translated) return translated;
  const char* fallback = generatedPingDialogLocaleString(0, id);
  return fallback ? fallback : "";
}

void PingDialog::setWaiting() {
  _text.clear();
  _text.addLine(trPingDialog(PingDialogTextId::PingDialogPinging));
}
void PingDialog::setTimeout() {
  _text.clear();
  _text.addLine(trPingDialog(PingDialogTextId::PingDialogNoResponse));
}

void PingDialog::setReplied(uint32_t rttMs, float snrUs, float snrThem) {
  _text.clear();
  _text.addf(trPingDialog(PingDialogTextId::PingDialogRoundTrip), rttMs);
  _text.addf(trPingDialog(PingDialogTextId::PingDialogSnrUs), (double)snrUs);
  _text.addf(trPingDialog(PingDialogTextId::PingDialogSnrThem), (double)snrThem);
}

void PingDialog::draw(Canvas& c, int x, int y, int w, int h) {
  Canvas view = c.region(x, y, w, h);
  Canvas box = drawModalChrome(view);
  const int pad = 4;
  _text.draw(box, pad, pad, box.width() - 2 * pad, box.height() - 2 * pad);
}

}  // namespace mishmesh
