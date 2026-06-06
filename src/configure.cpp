#include "configure.hpp"

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
  preferences.putString(PREF_GIF_FILENAME, gif_filename);
  preferences.putBool(PREF_ENABLE_DISPLAY, enable_display);
  preferences.putString(PREF_LYRIC_FILENAME, lyric_filename);
  preferences.putString(PREF_LYRIC_COLOR, lyric_color);
  preferences.putUInt(PREF_LYRIC_Y1, lyric_y1);
  preferences.putUInt(PREF_LYRIC_Y2, lyric_y2);
}

void Configure::load() {
  display_bright = preferences.getUInt(PREF_DISPLAY_BRIGHT, 32);
  auto_bright_min = preferences.getUInt(PREF_DISPLAY_ABC_MIN, 0);
  auto_bright_max = preferences.getUInt(PREF_DISPLAY_ABC_MAX, 0);
  wifi_ssid = preferences.getString(PREF_WIFI_SSID, "gif_backpack");
  wifi_pwd = preferences.getString(PREF_WIFI_PASSWORD, "12345678");
  gif_filename = preferences.getString(PREF_GIF_FILENAME, "bobo.gif");
  enable_display = preferences.getBool(PREF_ENABLE_DISPLAY, true);
  lyric_filename = preferences.getString(PREF_LYRIC_FILENAME, "");
  lyric_color = preferences.getString(PREF_LYRIC_COLOR, "#ffcc33");
  lyric_y1 = preferences.getUInt(PREF_LYRIC_Y1, 12);
  lyric_y2 = preferences.getUInt(PREF_LYRIC_Y2, 36);
}
