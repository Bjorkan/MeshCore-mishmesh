void MessageThreadApplet::openQuickReplies() {
  _qrModel = QuickReplyModel{};
  _menu.setModel(&_qrModel);
  _menu.setRowHeight(14);
  _menu.setEmptyText(trMessageThread(MessageThreadTextId::MessageThreadNoQuickReplies));
  _menu.resetSelection();
  _qrOpen = true;
}

void MessageThreadApplet::startCompose(const char* seed) {
  if (seed && seed[0]) { strncpy(_composeBuf, seed, KeypadApplet::KP_MAX);
                         _composeBuf[KeypadApplet::KP_MAX] = 0; }
  else _composeBuf[0] = 0;
  keypadApplet().configure(_composeBuf, KeypadApplet::KP_MAX,
                           trMessageThread(MessageThreadTextId::MessageThreadMessage),
                           &MessageThreadApplet::onComposeDone, this);
  if (_host) _host->push(&keypadApplet());
}

void MessageThreadApplet::onComposeDone(void* ctx, const char* text) {
  auto* self = static_cast<MessageThreadApplet*>(ctx);
  if (!text || !text[0] || !self->_svc) return;          // empty -> send nothing
  if (!self->_svc->sendText(self->_key, text)) {
    if (self->_host)
      self->_host->postToast(trMessageThread(MessageThreadTextId::MessageThreadSendFailed));
  }
}

void MessageThreadApplet::refreshRegion() {
  _regionBuf[0] = 0;
  if (_svc) _svc->region(_key, _regionBuf, sizeof(_regionBuf));
  _chatMenu.setRegion(_regionBuf);   // shows "None" when empty
  if (_svc) _chatMenu.setNotifyLabel(mishmesh::notifyLevelShortLabel(_svc->notifyLevel(_key)));
}

void MessageThreadApplet::openRegionEditor() {
  _regionBuf[0] = 0;
  if (_svc) _svc->region(_key, _regionBuf, sizeof(_regionBuf));   // seed with current
  keypadApplet().configure(_regionBuf, sizeof(_regionBuf) - 1,
                           trMessageThread(MessageThreadTextId::MessageThreadRegion),
                           &MessageThreadApplet::onRegionDone, this);
  if (_host) _host->push(&keypadApplet());
}

void MessageThreadApplet::onRegionDone(void* ctx, const char* text) {
  auto* self = static_cast<MessageThreadApplet*>(ctx);
  if (!self->_svc) return;
  self->_svc->setRegion(self->_key, text);   // empty -> clears to "None"
  self->refreshRegion();
}

MessageThreadApplet& messageThreadApplet() {
  static MessageThreadApplet a;
  return a;
}

} // namespace mishmesh
