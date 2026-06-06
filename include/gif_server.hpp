#pragma once

#include <functional>
#include <WebServer.h>
#include <SPIFFS.h>

class GifServer {
 public:
  GifServer(int port) : server_(port) {}

  bool init(std::function<void()> reload_callback);

  void begin();
  void handle_client();

 private:
  WebServer server_;
  std::function<void()> reload_callback_ = nullptr;

  File upload_file_;

  void server_index();
  void handle_gif_file();
  void handle_filelist();
  void handle_upload();
  void handle_delete();
  void handle_config();
  void handle_api_gifs();
  void handle_api_songs();
  void handle_api_lyric();
  void handle_api_state();
};
