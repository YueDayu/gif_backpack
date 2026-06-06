#include <SPIFFS.h>

#include "ble_control.hpp"
#include "configure.hpp"
#include "gif_player.hpp"
#include "lyric_player.hpp"

BleControl ble_control;

void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);

  auto& config = Configure::instance();
  config.load();

  GifPlayer::instance().init();
  GifPlayer::instance().set_gif_file(config.gif_filename);
  LyricPlayer::instance().init();
  ble_control.begin([]() {
    GifPlayer::instance().set_gif_file(Configure::instance().gif_filename);
    LyricPlayer::instance().set_song_file(Configure::instance().lyric_filename);
  });
}

void loop() { GifPlayer::instance().update(); }
