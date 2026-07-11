#include <gtest/gtest.h>
#include <mishmesh/applets/settings/HomeSettingsPanel.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/Locale.h>
#include <mishmesh/core/UiPrefs.h>

#include <map>
#include <string>
#include <vector>
#include <string.h>

using namespace mishmesh;

namespace {

struct MemStorage : AppletStorage {
  std::map<std::string, std::vector<uint8_t>> kv;

  uint8_t load(const char* key, uint8_t* dst, uint8_t cap) override {
    auto it = kv.find(key);
    if (it == kv.end()) return 0;
    uint8_t n = (uint8_t)(it->second.size() < cap ? it->second.size() : cap);
    memcpy(dst, it->second.data(), n);
    return n;
  }

  bool save(const char* key, const uint8_t* src, uint8_t len) override {
    kv[key] = std::vector<uint8_t>(src, src + len);
    return true;
  }
};

void resetLocale() {
  uiPrefs().resetForTest();
  uiPrefs().begin(nullptr);
}

}  // namespace

TEST(Locale, DefaultsToEnglish) {
  resetLocale();
  EXPECT_STREQ("en_US", localeManager().current().tag);
  EXPECT_STREQ("Settings", tr(TextId::AppSettings));
}

TEST(Locale, SelectsSwedishByTag) {
  resetLocale();
  ASSERT_TRUE(localeManager().setCurrent("sv_SE"));
  EXPECT_STREQ("Svenska", localeManager().current().name);
  EXPECT_STREQ("Inställningar", tr(TextId::AppSettings));
  EXPECT_STREQ("Skärmsläckning", tr(TextId::HomeScreenSleep));
  EXPECT_FALSE(localeManager().setCurrent("not-a-locale"));
}

TEST(Locale, PersistsSelection) {
  MemStorage storage;
  localeManager().resetForTest();
  localeManager().begin(&storage);
  ASSERT_TRUE(localeManager().setCurrent("sv_SE"));

  localeManager().resetForTest();
  localeManager().begin(&storage);
  EXPECT_STREQ("sv_SE", localeManager().current().tag);
  EXPECT_STREQ("Språk", tr(TextId::SettingsLanguage));
}

TEST(HomeSettingsPanel, SelectsLanguageFromHomeSettings) {
  resetLocale();
  HomeSettingsPanel& panel = homeSettings();
  AppletContext ctx;
  panel.begin(ctx);

  for (int i = 0; i < 4; ++i)
    EXPECT_TRUE(panel.onInput(InputEvent::NavDown));
  EXPECT_TRUE(panel.onInput(InputEvent::Select));
  ASSERT_TRUE(panel.modalActive());
  EXPECT_TRUE(panel.onInput(InputEvent::NavRight));
  EXPECT_TRUE(panel.onInput(InputEvent::Select));
  EXPECT_FALSE(panel.modalActive());
  EXPECT_STREQ("sv_SE", localeManager().current().tag);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
