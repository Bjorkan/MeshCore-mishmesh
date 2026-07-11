bool MessageThreadApplet::onInput(InputEvent ev) {
  // The per-message overlay swallows everything until it closes.
  if (_menuOpen) {
    if (_menu.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      switch (_msgMenu.actionAt(_menu.selected())) {
        case MsgMenuModel::Delete:
          if (_svc) _svc->deleteMessage(_key, _focus);
          _menuOpen = false;
          break;
        case MsgMenuModel::Path:
          _menuOpen = false;
          messagePathApplet().setTarget(_key, _focus);
          if (_host) _host->push(&messagePathApplet());
          break;
        case MsgMenuModel::Reply: {            // compose, seeding @[sender] for channels
          _menuOpen = false;
          char seed[48] = {0};
          MessageView m;
          if (_key.type == 1 && _svc && _svc->getMessage(_key, _focus, m) &&
              m.senderName && m.senderName[0])
            snprintf(seed, sizeof(seed), "@[%s] ", m.senderName);   // app's mention format
          startCompose(seed);
          break;
        }
        case MsgMenuModel::Resend: {           // re-send the body as a fresh outbound message
          _menuOpen = false;
          char body[mishmesh::MAX_TEXT + 1] = {0};
          MessageView m;
          if (_svc && _svc->getMessage(_key, _focus, m) && m.text) {
            snprintf(body, sizeof(body), "%s", m.text);
            bool ok = body[0] && _svc->sendText(_key, body);
            if (_host) _host->postToast(ok
                ? trMessageThread(MessageThreadTextId::MessageThreadResending)
                : trMessageThread(MessageThreadTextId::MessageThreadResendFailed));
          }
          break;
        }
      }
      return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _menuOpen = false; return true; }
    return true;   // swallow all input while the menu is open
  }
  if (_qrOpen) {
    if (_menu.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      const char* txt = quickReplyStore().text(_menu.selected());
      if (txt && txt[0]) onComposeDone(this, txt);   // identical to a typed send
      _qrOpen = false;
      return true;
    }
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { _qrOpen = false; return true; }
    return true;   // modal swallows everything else
  }

  // The chat menu's confirm dialog is a modal: once armed it captures all input
  // ahead of the tab bar, same modal-first rule every screen follows (cf.
  // ContactsApplet guarding its confirm before its tabs). Without this the tab
  // bar below would eat NavLeft/NavRight before the dialog's buttons saw them.
  if (_chatMenu.modalActive()) {
    _chatMenu.onInput(ev);
    const char* toast = nullptr;
    ChatMenu::Result r = _chatMenu.takeResult(toast);
    if (_host && toast) _host->postToast(toast);
    if (r == ChatMenu::Result::Deleted) { if (_host) _host->pop(); }    // chat is gone
    else if (r == ChatMenu::Result::Cleared) { _tabs.setSelected(0); _pinBottom = true; }
    return true;   // cancel just dismisses the dialog, back to the action list
  }

  // NavLeft/NavRight switch tabs (unused by the conversation view otherwise).
  if (_tabs.onInput(ev)) return true;

  // Settings tab: drive the shared chat menu.
  if (_tabs.selected() == 1) {
    if (_chatMenu.onInput(ev)) return true;
    if (ev == InputEvent::Select) {
      const char* toast = nullptr;
      ChatMenu::Result r = _chatMenu.activate(_svc, toast);   // Mark unread runs now; Clear/Delete arm a modal; Notify opens the notify applet
      if (r == ChatMenu::Result::EditRegion) { openRegionEditor(); return true; }   // keypad over the menu
      if (r == ChatMenu::Result::EditNotify) {
        chatNotifyApplet().setTarget(_key, resolveTitle());
        if (_host) _host->push(&chatNotifyApplet());
        return true;
      }
      if (r == ChatMenu::Result::Share) {
        channelShareApplet().setTarget(_key, resolveTitle());
        if (_host) _host->push(&channelShareApplet());
        return true;
      }
      if (_host && toast) _host->postToast(toast);
      if (r == ChatMenu::Result::Deleted) { if (_host) _host->pop(); }   // chat is gone
      else if (!_chatMenu.modalActive()) { _tabs.setSelected(0); _pinBottom = true; }  // non-destructive -> conversation
      return true;
    }
    return false;   // Back bubbles -> pop the thread
  }

  return onConversationInput(ev);
}

bool MessageThreadApplet::onConversationInput(InputEvent ev) {
  int n = _svc ? _svc->messageCount(_key) : 0;

  // Empty thread: the action bar is the only focusable region, and the n==0
  // render path never computes the message-layout fields the scroll logic below
  // relies on (which are stale from the previously-viewed thread - this applet is
  // a reused singleton). Keep focus inside the bar so Write stays reachable.
  if (n == 0) {
    if (ev == InputEvent::NavDown) { _barRow = (_barRow < 1) ? _barRow + 1 : 1; return true; }
    if (ev == InputEvent::NavUp)   { _barRow = (_barRow > 0) ? _barRow - 1 : 0; return true; }
    if (ev == InputEvent::Select) {
      if (_barRow <= 0) startCompose();
      else openQuickReplies();
      return true;
    }
    return false;   // Back bubbles -> pop to chat list (Left/Right consumed by tabs)
  }

  // Nav decisions use the committed target (not the eased value) to avoid chatter.
  int vp = _bodyH > 0 ? _bodyH : 48;
  int step = vp > 16 ? (vp * 2) / 3 : 12;

  // Button bar has focus: vertical nav moves between/out of the two buttons;
  // Left/Right and Back are left unconsumed so they bubble (tabs / pop).
  if (_barRow >= 0) {
    if (ev == InputEvent::NavDown) { if (_barRow < 1) _barRow++; return true; }
    if (ev == InputEvent::NavUp)   { _barRow = (_barRow > 0) ? _barRow - 1 : -1; return true; }
    if (ev == InputEvent::Select) {
      if (_barRow == 0) startCompose();
      else openQuickReplies();
      return true;
    }
    return false;
  }

  if (ev == InputEvent::NavUp) {
    if (_focusTop < _scrollTarget) {             // more of the focused message is above
      _scrollTarget -= step; if (_scrollTarget < _focusTop) _scrollTarget = _focusTop;
    } else if (_focus > 0) {
      _focus--;
    }
    return true;
  }
  if (ev == InputEvent::NavDown) {
    if (_focusBot > _scrollTarget + vp) {        // more of the focused message is below
      _scrollTarget += step; if (_scrollTarget > _focusBot - vp) _scrollTarget = _focusBot - vp;
    } else if (_focus < n - 1) {
      _focus++;
    } else {
      _barRow = 0;                               // past the last message -> action bar
    }
    return true;
  }
  if (ev == InputEvent::Select && n > 0) {
    MessageView m; bool hasPath = false, repeats = false, canResend = false;
    if (_svc && _svc->getMessage(_key, _focus, m)) {
      hasPath = (m.kind == KIND_INBOUND) || (m.kind == KIND_OUT_CHAN);
      repeats = (m.kind == KIND_OUT_CHAN);
      // Offer a manual resend where auto-retry can't help: a DM that ran out of
      // retries, or our own channel message nobody has echoed back yet.
      canResend = (m.kind == KIND_OUT_DM   && m.status == ST_FAILED) ||
                  (m.kind == KIND_OUT_CHAN && m.heardCount == 0);
    }
    _msgMenu.hasPath = hasPath;
    _msgMenu.repeats = repeats;
    _msgMenu.canResend = canResend;
    _menu.setModel(&_msgMenu); _menu.setRowHeight(14); _menu.resetSelection();
    _menuOpen = true;
    return true;
  }
  return false;   // Back bubbles -> pop to chat list
}

