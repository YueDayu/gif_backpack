#include "gif_server.hpp"

#include <SPIFFS.h>

#include <algorithm>
#include <vector>

#include "configure.hpp"
#include "lyric_player.hpp"

const String gif_basedir = "/gif";
const String lyric_basedir = "/lyrics";
const String lyric_pack_file = "/lyrics/lyrics.pack";
const String lyric_index_file = "/lyrics/index.txt";

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
  if (file && !file.isDirectory()) {
    yes = true;
  }
  file.close();
  return yes;
}

String json_escape(const String& input) {
  String output;
  output.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    switch (c) {
      case '"':
        output += "\\\"";
        break;
      case '\\':
        output += "\\\\";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        output += c;
        break;
    }
  }
  return output;
}

String base_name(const String& path) {
  const int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

bool GifServer::init(std::function<void()> reload_callback) {
  reload_callback_ = reload_callback;
  server_index();
  server_.serveStatic("/lyrics/", SPIFFS, "/lyrics/");
  server_.on("/filelist", HTTP_POST, [this]() { this->handle_filelist(); });
  server_.on("/gif", HTTP_GET, [this]() { this->handle_gif_file(); });
  server_.on(
      "/upload",
      HTTP_POST,
      [this]() { this->server_.send(200, "text/plain", ""); },
      [this]() { this->handle_upload(); });
  server_.on("/delete", HTTP_POST, [this]() { this->handle_delete(); });
  server_.on("/config", HTTP_POST, [this]() { this->handle_config(); });
  server_.on("/api/gifs", HTTP_GET, [this]() { this->handle_api_gifs(); });
  server_.on("/api/songs", HTTP_GET, [this]() { this->handle_api_songs(); });
  server_.on("/api/lyric", HTTP_GET, [this]() { this->handle_api_lyric(); });
  server_.on("/api/state", HTTP_GET, [this]() { this->handle_api_state(); });
  server_.on("/api/state", HTTP_POST, [this]() { this->handle_api_state(); });
  return true;
}

void GifServer::begin() { server_.begin(); }

void GifServer::handle_client() { server_.handleClient(); }

void GifServer::server_index() {
  auto send_index = [this]() {
    File file = SPIFFS.open("/web/index.html", "r");
    if (!file) {
      server_.send(404, "text/plain", "IndexNotFound");
      return;
    }
    server_.streamFile(file, "text/html");
    file.close();
  };
  server_.on("/", HTTP_GET, send_index);
  server_.on("/index.html", HTTP_GET, send_index);
}

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
    output += base_name(String(file.name()));
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

void GifServer::handle_api_gifs() {
  File root = SPIFFS.open(gif_basedir);
  String output = "{\"total\":";
  output += SPIFFS.totalBytes();
  output += ",\"used\":";
  output += SPIFFS.usedBytes();
  output += ",\"files\":[";

  File file = root.openNextFile();
  bool is_first = true;
  while (file) {
    if (!is_first) {
      output += ',';
    }
    String name = base_name(String(file.name()));
    output += "{\"name\":\"";
    output += json_escape(name);
    output += "\",\"size\":";
    output += file.size();
    output += "}";
    file = root.openNextFile();
    is_first = false;
  }
  output += "]}";
  server_.send(200, "application/json", output);
}

void GifServer::handle_api_songs() {
  if (!exists(lyric_index_file)) {
    server_.send(200, "application/json", "[]");
    return;
  }

  String query = server_.arg("q");
  query.toLowerCase();
  int limit = server_.hasArg("limit") ? server_.arg("limit").toInt() : 40;
  limit = std::max(1, std::min(80, limit));

  File index = SPIFFS.open(lyric_index_file, "r");
  String output = "[";
  bool is_first = true;
  int emitted = 0;
  while (index.available() && emitted < limit) {
    String line = index.readStringUntil('\n');
    line.trim();
    const int first_tab = line.indexOf('\t');
    const int second_tab = line.indexOf('\t', first_tab + 1);
    const int third_tab = line.indexOf('\t', second_tab + 1);
    if (first_tab < 0 || second_tab < 0 || third_tab < 0) {
      continue;
    }

    const String filename = line.substring(0, first_tab);
    const String title = line.substring(third_tab + 1);
    String searchable = filename + " " + title;
    searchable.toLowerCase();
    if (query.length() > 0 && searchable.indexOf(query) < 0) {
      continue;
    }

    if (!is_first) {
      output += ",";
    }
    output += "{\"file\":\"";
    output += json_escape(filename);
    output += "\",\"title\":\"";
    output += json_escape(title);
    output += "\",\"artist\":\"五月天\"}";
    is_first = false;
    emitted++;
  }
  index.close();
  output += "]";
  server_.send(200, "application/json", output);
}

void GifServer::handle_api_lyric() {
  if (!server_.hasArg("file")) {
    server_.send(400, "text/plain", "Missing file");
    return;
  }

  const String filename = server_.arg("file");
  const String direct_path = lyric_basedir + "/" + filename;
  if (exists(direct_path)) {
    File file = SPIFFS.open(direct_path, "r");
    server_.streamFile(file, "text/plain; charset=utf-8");
    file.close();
    return;
  }

  File index = SPIFFS.open(lyric_index_file, "r");
  if (!index) {
    server_.send(404, "text/plain", "LyricIndexNotFound");
    return;
  }

  uint32_t offset = 0;
  uint32_t length = 0;
  const String prefix = filename + "\t";
  while (index.available()) {
    String line = index.readStringUntil('\n');
    line.trim();
    if (!line.startsWith(prefix)) {
      continue;
    }
    const int first_tab = line.indexOf('\t');
    const int second_tab = line.indexOf('\t', first_tab + 1);
    const int third_tab = line.indexOf('\t', second_tab + 1);
    if (first_tab < 0 || second_tab < 0 || third_tab < 0) {
      continue;
    }
    offset = line.substring(first_tab + 1, second_tab).toInt();
    length = line.substring(second_tab + 1, third_tab).toInt();
    break;
  }
  index.close();

  if (length == 0) {
    server_.send(404, "text/plain", "LyricNotFound");
    return;
  }

  File pack = SPIFFS.open(lyric_pack_file, "r");
  if (!pack || !pack.seek(offset)) {
    server_.send(404, "text/plain", "LyricPackNotFound");
    return;
  }

  std::vector<char> buffer(length + 1);
  const size_t read_bytes = pack.readBytes(buffer.data(), length);
  pack.close();
  if (read_bytes != length) {
    server_.send(500, "text/plain", "LyricReadFailed");
    return;
  }
  buffer[length] = '\0';
  server_.send(200, "text/plain; charset=utf-8", buffer.data());
}

void GifServer::handle_api_state() {
  auto& config = Configure::instance();
  bool needs_save = false;
  bool needs_reload = false;
  uint32_t requested_progress = LyricPlayer::instance().current_progress_ms();
  bool has_progress = false;

  if (server_.method() == HTTP_POST) {
    if (server_.hasArg("bright")) {
      const int value = server_.arg("bright").toInt();
      config.display_bright = std::max(0, std::min(100, value));
      needs_save = true;
    }
    if (server_.hasArg("enable")) {
      String value = server_.arg("enable");
      config.enable_display = value == "1" || value == "true";
      needs_save = true;
    }
    if (server_.hasArg("gif")) {
      String value = server_.arg("gif");
      if (value != config.gif_filename) {
        config.gif_filename = value;
        needs_save = true;
        needs_reload = true;
      }
    }
    if (server_.hasArg("song")) {
      String value = server_.arg("song");
      if (value != config.lyric_filename) {
        config.lyric_filename = value;
        needs_save = true;
        needs_reload = true;
        requested_progress = 0;
        has_progress = true;
      }
    }
    if (server_.hasArg("color")) {
      String value = server_.arg("color");
      value.trim();
      if (value.length() == 7 && value.startsWith("#")) {
        config.lyric_color = value;
        needs_save = true;
      }
    }
    if (server_.hasArg("y1")) {
      const int value = server_.arg("y1").toInt();
      config.lyric_y1 = std::max(0, std::min(48, value));
      needs_save = true;
    }
    if (server_.hasArg("y2")) {
      const int value = server_.arg("y2").toInt();
      config.lyric_y2 = std::max(0, std::min(48, value));
      needs_save = true;
    }
    if (server_.hasArg("progress")) {
      requested_progress = std::max(0, int(server_.arg("progress").toInt()));
      has_progress = true;
    }

    if (needs_save) {
      config.save();
    }
    if (needs_reload && reload_callback_ != nullptr) {
      reload_callback_();
    }
    if (has_progress) {
      LyricPlayer::instance().set_progress(requested_progress);
    }
  }

  String output = "{";
  output += "\"bright\":";
  output += config.display_bright;
  output += ",\"enable\":";
  output += config.enable_display ? "true" : "false";
  output += ",\"gif\":\"";
  output += json_escape(config.gif_filename);
  output += "\",\"song\":\"";
  output += json_escape(config.lyric_filename);
  output += "\",\"progressMs\":";
  output += LyricPlayer::instance().current_progress_ms();
  output += ",\"color\":\"";
  output += json_escape(config.lyric_color);
  output += "\",\"y1\":";
  output += config.lyric_y1;
  output += ",\"y2\":";
  output += config.lyric_y2;
  output += "}";
  server_.send(200, "application/json", output);
}
