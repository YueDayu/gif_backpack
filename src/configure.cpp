#include "configure.hpp"

namespace {
uint32_t get_uint_or_default(Preferences& preferences, const char* key, uint32_t default_value) {
  if (!preferences.isKey(key)) {
    preferences.putUInt(key, default_value);
    return default_value;
  }
  return preferences.getUInt(key, default_value);
}

bool get_bool_or_default(Preferences& preferences, const char* key, bool default_value) {
  if (!preferences.isKey(key)) {
    preferences.putBool(key, default_value);
    return default_value;
  }
  return preferences.getBool(key, default_value);
}

String get_string_or_default(Preferences& preferences, const char* key, const char* default_value) {
  if (!preferences.isKey(key)) {
    preferences.putString(key, default_value);
    return String(default_value);
  }
  return preferences.getString(key, default_value);
}
}  // namespace

Configure& Configure::instance() {
  static Configure config;
  return config;
}

Configure::Configure() {
  preferences.begin(PREF_DB_NAME, false);
  load();
}

void Configure::save() {
  preferences.putUInt(PREF_DISPLAY_BRIGHT, display_bright);
  preferences.putUInt(PREF_DISPLAY_ABC_MIN, auto_bright_min);
  preferences.putUInt(PREF_DISPLAY_ABC_MAX, auto_bright_max);
  preferences.putString(PREF_WIFI_SSID, wifi_ssid);
  preferences.putString(PREF_WIFI_PASSWORD, wifi_pwd);
  preferences.putString(PREF_DISPLAY_MODE, display_mode);
  preferences.putString(PREF_GIF_FILENAME, gif_filename);
  preferences.putBool(PREF_ENABLE_DISPLAY, enable_display);
  preferences.putString(PREF_LYRIC_FILENAME, lyric_filename);
  preferences.putString(PREF_LYRIC_COLOR, lyric_color);
  preferences.putString(PREF_LYRIC_NEXT_COLOR, lyric_next_color);
  preferences.putUInt(PREF_LYRIC_Y1, lyric_y1);
  preferences.putUInt(PREF_LYRIC_Y2, lyric_y2);
}

void Configure::load() {
  display_bright = get_uint_or_default(preferences, PREF_DISPLAY_BRIGHT, 32);
  auto_bright_min = get_uint_or_default(preferences, PREF_DISPLAY_ABC_MIN, 0);
  auto_bright_max = get_uint_or_default(preferences, PREF_DISPLAY_ABC_MAX, 0);
  wifi_ssid = get_string_or_default(preferences, PREF_WIFI_SSID, "gif_backpack");
  wifi_pwd = get_string_or_default(preferences, PREF_WIFI_PASSWORD, "12345678");
  display_mode = get_string_or_default(preferences, PREF_DISPLAY_MODE, "gif");
  if (display_mode != "gif" && display_mode != "lyrics") {
    display_mode = "gif";
  }
  gif_filename = get_string_or_default(preferences, PREF_GIF_FILENAME, "bobo.gif");
  enable_display = get_bool_or_default(preferences, PREF_ENABLE_DISPLAY, true);
  lyric_filename = get_string_or_default(preferences, PREF_LYRIC_FILENAME, "");
  lyric_color = get_string_or_default(preferences, PREF_LYRIC_COLOR, "#ffcc33");
  lyric_next_color = get_string_or_default(preferences, PREF_LYRIC_NEXT_COLOR, "#7bdcff");
  lyric_y1 = get_uint_or_default(preferences, PREF_LYRIC_Y1, 12);
  lyric_y2 = get_uint_or_default(preferences, PREF_LYRIC_Y2, 36);
}
