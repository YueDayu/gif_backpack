#include "gif_server.hpp"

#include <SPIFFS.h>

#include "configure.hpp"

const String gif_basedir = "/gif";

// config str: [bright+1] [enable_display+1] [len_ssid+1][len_pwd+1][len_filename+1]
// [wifi_ssid][wifi_pwd][filename]
String config_to_str() {
  const auto& config = Configure::instance();
  String output;
  output.reserve(5 + config.wifi_ssid.length() + config.wifi_pwd.length() +
                 config.gif_filename.length());
  output += "12345";
  output[0] = config.display_bright + 1;
  output[1] = uint8_t(config.enable_display) + 1;
  output[2] = uint8_t(config.wifi_ssid.length()) + 1;
  output[3] = uint8_t(config.wifi_pwd.length()) + 1;
  output[4] = uint8_t(config.gif_filename.length()) + 1;
  output += config.wifi_ssid;
  output += config.wifi_pwd;
  output += config.gif_filename;
  return output;
}

bool set_config_from_str(const String& str, bool save_config = true) {
  auto& config = Configure::instance();
  config.display_bright = uint8_t(str[0]) - 1;
  config.enable_display = bool(uint8_t(str[1]) - 1);
  uint32_t cur_pos = 5;
  uint8_t len = uint8_t(str[2]) - 1;
  if (len > 0) {
    config.wifi_ssid = str.substring(cur_pos, cur_pos + len);
    cur_pos += len;
  }
  len = uint8_t(str[3]) - 1;
  if (len > 1) {
    config.wifi_pwd = str.substring(cur_pos, cur_pos + len);
    cur_pos += len;
  }
  bool gif_changed = false;
  len = uint8_t(str[4]) - 1;
  if (len) {
    config.gif_filename = str.substring(cur_pos, cur_pos + len);
    gif_changed = true;
  }
  if (save_config) {
    config.save();
  }
  return gif_changed;
}

bool exists(String path) {
  bool yes = false;
  File file = SPIFFS.open(path, "r");
  if (!file.isDirectory()) {
    yes = true;
  }
  file.close();
  return yes;
}

bool GifServer::init(std::function<void()> reload_callback) {
  reload_callback_ = reload_callback;
  server_index();
  server_.on("/filelist", HTTP_POST, [this]() { this->handle_filelist(); });
  server_.on("/gif", HTTP_GET, [this]() { this->handle_gif_file(); });
  server_.on(
      "/upload",
      HTTP_POST,
      [this]() { this->server_.send(200, "text/plain", ""); },
      [this]() { this->handle_upload(); });
  server_.on("/delete", HTTP_POST, [this]() { this->handle_delete(); });
  server_.on("/config", HTTP_POST, [this]() { this->handle_config(); });
  return true;
}

void GifServer::begin() { server_.begin(); }

void GifServer::handle_client() { server_.handleClient(); }

void GifServer::server_index() { server_.serveStatic("/", SPIFFS, "/web/index.html"); }

void GifServer::handle_gif_file() {
  String gif_file = gif_basedir + '/' + server_.arg("img");
  if (exists(gif_file)) {
    File file = SPIFFS.open(gif_file, "r");
    server_.streamFile(file, "image/gif");
    file.close();
    return;
  }
  server_.send(404, "text/plain", "FileNotFound");
}

void GifServer::handle_filelist() {
  File root = SPIFFS.open(gif_basedir);
  String output = "{\"t\":";
  output += SPIFFS.totalBytes();
  output += ",\"u\":";
  output += SPIFFS.usedBytes();
  output += ",\"fl\":[";
  File file = root.openNextFile();
  bool is_first = true;
  while (file) {
    if (!is_first) {
      output += ',';
    }
    output += "{\"sz\":\"";
    output += file.size();
    output += "\",\"nm\":\"";
    output += String(file.name());
    output += "\"}";
    file = root.openNextFile();
    is_first = false;
  }
  output += "]}";
  server_.send(200, "text/json", output);
}

void GifServer::handle_upload() {
  HTTPUpload& upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    filename = gif_basedir + filename;
    upload_file_ = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (upload_file_) {
      upload_file_.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (upload_file_) {
      upload_file_.close();
    }
  }
}

void GifServer::handle_delete() {
  if (server_.args() == 0) {
    return server_.send(500, "text/plain", "BAD ARGS");
  }
  String path = gif_basedir + '/' + server_.arg(0);
  if (!exists(path)) {
    return server_.send(404, "text/plain", "FileNotFound");
  }
  SPIFFS.remove(path);
  handle_filelist();
}

void GifServer::handle_config() {
  if (server_.args() != 0) {
    if (set_config_from_str(server_.arg(0), true) && reload_callback_ != nullptr) {
      reload_callback_();
    }
  }
  server_.send(200, "text/plain", config_to_str());
}
