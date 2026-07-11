#pragma once

#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/LocaleStrings.h>

#include <stdint.h>
#include <string.h>

namespace mishmesh {

// Runtime locale selection backed by generated, flash-resident string tables.
// Locale index 0 (en_US) is always the fallback for missing translations.
class LocaleManager {
public:
  void begin(AppletStorage* storage) {
    _storage = storage;
    _current = 0;
    if (!_storage) return;

    uint8_t stored = 0;
    if (_storage->load(storageKey(), &stored, 1) == 1 && stored < count())
      _current = stored;
  }

  uint8_t currentIndex() const { return _current; }
  uint8_t count() const { return generatedLocaleCount(); }
  const LocaleDescriptor& current() const { return descriptor(_current); }
  const LocaleDescriptor& descriptor(uint8_t index) const {
    return generatedLocaleDescriptor(index);
  }

  bool setCurrent(uint8_t index) {
    if (index >= count()) return false;
    _current = index;
    if (_storage) _storage->save(storageKey(), &_current, 1);
    return true;
  }

  bool setCurrent(const char* tag) {
    if (!tag) return false;
    for (uint8_t i = 0; i < count(); ++i) {
      if (strcmp(descriptor(i).tag, tag) == 0) return setCurrent(i);
    }
    return false;
  }

  const char* text(TextId id) const {
    const char* translated = generatedLocaleString(_current, id);
    if (translated) return translated;
    const char* fallback = generatedLocaleString(0, id);
    return fallback ? fallback : "";
  }

  void resetForTest() {
    _storage = nullptr;
    _current = 0;
  }

private:
  static const char* storageKey() { return "uilang"; }

  AppletStorage* _storage = nullptr;
  uint8_t _current = 0;
};

inline LocaleManager& localeManager() {
  static LocaleManager manager;
  return manager;
}

inline const char* tr(TextId id) {
  return localeManager().text(id);
}

}  // namespace mishmesh
