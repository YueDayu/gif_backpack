#pragma once

#include <Preferences.h>

#define PREF_DB_NAME "backpack"

class Configure {
 public:
  static Configure& instance();

  void save();
  void load();

 private:
  Configure();

  Preferences preferences;

 public:
  const char* const PREF_DISPLAY_BRIGHT = "displayBright";
  const char* const PREF_DISPLAY_ABC_MIN = "autoBrightMin";
  const char* const PREF_DISPLAY_ABC_MAX = "autoBrightMax";
  const char* const PREF_WIFI_SSID = "wifiSsid";
  const char* const PREF_WIFI_PASSWORD = "wifiPwd";
  const char* const PREF_GIF_FILENAME = "filename";
  const char* const PREF_ENABLE_DISPLAY = "enable";
  const char* const PREF_LYRIC_FILENAME = "lyricFile";
  const char* const PREF_LYRIC_COLOR = "lyricColor";
  const char* const PREF_LYRIC_NEXT_COLOR = "lyricNextColor";
  const char* const PREF_LYRIC_Y1 = "lyricY1";
  const char* const PREF_LYRIC_Y2 = "lyricY2";

  uint8_t display_bright;
  uint16_t auto_bright_min;
  uint16_t auto_bright_max;
  String wifi_ssid;
  String wifi_pwd;
  String gif_filename;
  bool enable_display;
  String lyric_filename;
  String lyric_color;
  String lyric_next_color;
  uint8_t lyric_y1;
  uint8_t lyric_y2;
};
