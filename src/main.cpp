#include <DNSServer.h>
#include <SPIFFS.h>
#include <WiFi.h>

#include "configure.hpp"
#include "gif_player.hpp"
#include "gif_server.hpp"
#include "lyric_player.hpp"

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);

GifServer server(80);

void server_task(void*) {
  while (true) {
    server.handle_client();
  }
}

void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);

  auto& config = Configure::instance();
  config.load();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(config.wifi_ssid, config.wifi_pwd, 13, 0, 1);

  GifPlayer::instance().init();
  GifPlayer::instance().set_gif_file(config.gif_filename);
  LyricPlayer::instance().init();
  server.init([]() {
    GifPlayer::instance().set_gif_file(Configure::instance().gif_filename);
    LyricPlayer::instance().set_song_file(Configure::instance().lyric_filename);
  });
  server.begin();

  xTaskCreatePinnedToCore(server_task, "server_task", 4096, nullptr, 1, nullptr, 0);
}

void loop() { GifPlayer::instance().update(); }
