// mishmesh/widgets/ChatMenu.cpp
#include <mishmesh/widgets/ChatMenu.h>
#include <mishmesh/core/Locale.h>
#include <stdio.h>

namespace mishmesh {

const char* chatMenuText(ChatMenuTextId id) {
  const uint8_t locale = localeManager().currentIndex();
  const char* translated = generatedChatMenuLocaleString(locale, id);
  if (translated) return translated;
  const char* fallback = generatedChatMenuLocaleString(0, id);
  return fallback ? fallback : "";
}

ChatMenu::Result ChatMenu::activate(MessagesService* svc, const char*& toast) {
  toast = nullptr;
  _svc = svc;
  if (!svc) return Result::None;
  switch (_model.actionAt(_menu.selected())) {
    case Model::ARegion: return Result::EditRegion;   // caller opens the region editor (keypad)
    case Model::ANotify: return Result::EditNotify;   // caller pushes ChatNotifyApplet
    case Model::AShare:  return Result::Share;         // caller pushes ChannelShareApplet
    case Model::AMarkUnread:
      svc->markUnread(_key);
      toast = chatMenuText(ChatMenuTextId::ChatMenuMarkedUnread);
      return Result::None;
    case Model::AClear:
      _confirm.configure(chatMenuText(ChatMenuTextId::ChatMenuConfirmClear));
      _confirming = true;
      return Result::None;
    default:
      _confirm.configure(chatMenuText(ChatMenuTextId::ChatMenuConfirmDelete));
      _confirming = true;
      return Result::None;
  }
}

bool ChatMenu::onInput(InputEvent ev) {
  if (!_confirming) return _menu.onInput(ev);

  if (_confirm.onInput(ev)) {
    ConfirmResult r = _confirm.result();
    if (r == ConfirmResult::Confirmed) {
      if (_model.actionAt(_menu.selected()) == Model::AClear) {   // Clear chat
        if (_svc) _svc->clearConvo(_key);
        _toast = chatMenuText(ChatMenuTextId::ChatMenuCleared); _pending = Result::Cleared;
      } else {                            // Delete chat
        if (_svc) _svc->deleteConvo(_key);
        _toast = chatMenuText(ChatMenuTextId::ChatMenuDeleted); _pending = Result::Deleted;
      }
      _confirming = false; _confirm.reset();
    } else if (r == ConfirmResult::Cancelled) {
      _confirming = false; _confirm.reset();   // back to the action list, nothing done
    }
  }
  return true;   // swallow all input while the dialog is up
}

}  // namespace mishmesh
