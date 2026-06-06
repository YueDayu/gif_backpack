#include "ble_control.hpp"

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <SPIFFS.h>

#include <algorithm>
#include <cctype>

#include "configure.hpp"
#include "gif_player.hpp"
#include "lyric_player.hpp"

namespace {
constexpr char kDeviceName[] = "gif_backpack";
constexpr char kServiceUuid[] = "6f8c0001-5d2f-4e3c-9f7a-7a1f2e8b0001";
constexpr char kCommandUuid[] = "6f8c0002-5d2f-4e3c-9f7a-7a1f2e8b0001";
constexpr char kStateUuid[] = "6f8c0003-5d2f-4e3c-9f7a-7a1f2e8b0001";
constexpr uint32_t kSecuritySchemaVersion = 2;
constexpr size_t kMaxCommandBuffer = 512;
constexpr size_t kNotifyChunkSize = 20;
constexpr uint32_t kNotifyChunkDelayMs = 15;
constexpr char kLyricIndexFile[] = "/lyrics/index.txt";

class CommandCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit CommandCallbacks(BleControl* control) : control_(control) {}

  void onWrite(NimBLECharacteristic* characteristic) override {
    if (control_ == nullptr) {
      return;
    }
    const std::string value = characteristic->getValue();
    if (value.empty()) {
      return;
    }

    String fragment;
    fragment.reserve(value.size());
    for (char c : value) {
      fragment += c;
    }
    control_->append_command_fragment(fragment);
  }

 private:
  BleControl* control_;
};
}  // namespace

bool BleControl::begin(std::function<void()> reload_callback) {
  reload_callback_ = reload_callback;

  NimBLEDevice::init(kDeviceName);
  Preferences ble_preferences;
  if (ble_preferences.begin("ble_control", false)) {
    const uint32_t saved_version = ble_preferences.getUInt("sec_schema", 0);
    if (saved_version != kSecuritySchemaVersion) {
      NimBLEDevice::deleteAllBonds();
      ble_preferences.putUInt("sec_schema", kSecuritySchemaVersion);
    }
    ble_preferences.end();
  }
  NimBLEDevice::setMTU(185);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  NimBLEServer* server = NimBLEDevice::createServer();
  NimBLEService* service = server->createService(kServiceUuid);
  NimBLECharacteristic* command_characteristic = service->createCharacteristic(
      kCommandUuid,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC);
  state_characteristic_ = service->createCharacteristic(
      kStateUuid,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_ENC);

  command_characteristic->setCallbacks(new CommandCallbacks(this));
  state_characteristic_->setValue(build_state_line().c_str());

  service->start();
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);

  NimBLEAdvertisementData scan_response;
  scan_response.setName(kDeviceName);
  advertising->setScanResponseData(scan_response);
  advertising->setScanResponse(true);
  NimBLEDevice::startAdvertising();
  return true;
}

void BleControl::append_command_fragment(const String& fragment) {
  command_buffer_ += fragment;
  if (command_buffer_.length() > kMaxCommandBuffer) {
    command_buffer_ = "";
    notify_line("ERR command_too_long");
    return;
  }

  int newline = command_buffer_.indexOf('\n');
  while (newline >= 0) {
    String command = command_buffer_.substring(0, newline);
    command_buffer_.remove(0, newline + 1);
    command.trim();
    if (command.length() > 0) {
      handle_command(command);
    }
    newline = command_buffer_.indexOf('\n');
  }
}

void BleControl::handle_command(const String& command) {
  if (command == "STATE?") {
    notify_state();
    return;
  }
  if (command == "GIFS?") {
    handle_gif_list_request();
    return;
  }
  if (command == "SONGS?") {
    handle_song_list_request();
    return;
  }
  if (command.startsWith("LYRIC? ")) {
    handle_lyric_request(command.substring(7));
    return;
  }
  if (command.startsWith("SET ")) {
    handle_set_command(command.substring(4));
    return;
  }
  if (command.startsWith("UPLOAD_BEGIN ")) {
    handle_upload_begin(command.substring(13));
    return;
  }
  if (command.startsWith("UPLOAD_DATA ")) {
    handle_upload_data(command.substring(12));
    return;
  }
  if (command == "UPLOAD_END") {
    handle_upload_end();
    return;
  }
  if (command == "UPLOAD_ABORT") {
    handle_upload_abort();
    return;
  }
  notify_line("ERR unknown_command");
}

void BleControl::handle_set_command(const String& args) {
  auto& config = Configure::instance();
  bool needs_save = false;
  bool needs_reload = false;
  bool song_specified = false;
  bool has_progress = false;
  uint32_t requested_progress = LyricPlayer::instance().current_progress_ms();

  String value = arg_value(args, "bright");
  if (value.length() > 0) {
    config.display_bright = std::max<int>(0, std::min<int>(100, int(value.toInt())));
    needs_save = true;
  }

  value = arg_value(args, "enable");
  if (value.length() > 0) {
    config.enable_display = value == "1" || value == "true";
    needs_save = true;
  }

  value = arg_value(args, "mode");
  if ((value == "gif" || value == "lyrics") && value != config.display_mode) {
    config.display_mode = value;
    needs_save = true;
  }

  value = arg_value(args, "gif");
  if (value.length() > 0 && value != config.gif_filename) {
    config.gif_filename = value == "-" ? "" : value;
    needs_save = true;
    needs_reload = true;
  }

  value = arg_value(args, "song");
  if (value.length() > 0) {
    song_specified = true;
    const String lyric_filename = value == "-" ? "" : value;
    if (lyric_filename != config.lyric_filename) {
      config.lyric_filename = lyric_filename;
      needs_save = true;
      needs_reload = true;
    }
    requested_progress = 0;
    has_progress = true;
  }

  value = arg_value(args, "preview");
  if (value.length() > 0) {
    LyricPlayer::instance().set_preview_text(value);
  }

  value = arg_value(args, "color");
  if (value.length() > 0) {
    value.trim();
    if (value.length() == 7 && value.startsWith("#")) {
      config.lyric_color = value;
      needs_save = true;
    }
  }

  value = arg_value(args, "nextColor");
  if (value.length() > 0) {
    value.trim();
    if (value.length() == 7 && value.startsWith("#")) {
      config.lyric_next_color = value;
      needs_save = true;
    }
  }

  value = arg_value(args, "y1");
  if (value.length() > 0) {
    config.lyric_y1 = std::max<int>(0, std::min<int>(48, int(value.toInt())));
    needs_save = true;
  }

  value = arg_value(args, "y2");
  if (value.length() > 0) {
    config.lyric_y2 = std::max<int>(0, std::min<int>(48, int(value.toInt())));
    needs_save = true;
  }

  value = arg_value(args, "progress");
  if (value.length() > 0) {
    requested_progress = uint32_t(std::max<long>(0, value.toInt()));
    has_progress = true;
  }

  if (needs_save) {
    config.save();
  }
  if (needs_reload && reload_callback_ != nullptr) {
    reload_callback_();
  }
  if (song_specified && !needs_reload) {
    LyricPlayer::instance().set_song_file(config.lyric_filename);
  }
  if (has_progress) {
    LyricPlayer::instance().set_progress(requested_progress);
  }

  notify_state();
}

void BleControl::handle_upload_begin(const String& args) {
  handle_upload_abort();

  const String filename = sanitize_gif_filename(arg_value(args, "name"));
  const uint32_t size = uint32_t(std::max<long>(0, arg_value(args, "size").toInt()));
  if (filename.length() == 0 || size == 0) {
    notify_line("ERR upload_bad_request");
    return;
  }

  const size_t total = SPIFFS.totalBytes();
  const size_t used = SPIFFS.usedBytes();
  const size_t free_bytes = total > used ? total - used : 0;
  if (size > free_bytes + 4096) {
    notify_line("ERR upload_no_space");
    return;
  }

  const String path = String("/gif/") + filename;
  SPIFFS.remove(path);
  upload_file_ = SPIFFS.open(path, "w");
  if (!upload_file_) {
    notify_line("ERR upload_open_failed");
    return;
  }

  upload_filename_ = filename;
  upload_expected_size_ = size;
  upload_received_size_ = 0;
  upload_active_ = true;
  notify_line("UPLOAD_READY name=" + filename + "&size=" + String(size));
}

void BleControl::handle_upload_data(const String& args) {
  if (!upload_active_ || !upload_file_) {
    notify_line("ERR upload_not_active");
    return;
  }

  std::vector<uint8_t> bytes;
  if (!base64_decode(arg_value(args, "data"), bytes)) {
    handle_upload_abort();
    notify_line("ERR upload_bad_data");
    return;
  }
  if (upload_received_size_ + bytes.size() > upload_expected_size_) {
    handle_upload_abort();
    notify_line("ERR upload_too_large");
    return;
  }

  const size_t written = upload_file_.write(bytes.data(), bytes.size());
  if (written != bytes.size()) {
    handle_upload_abort();
    notify_line("ERR upload_write_failed");
    return;
  }
  upload_received_size_ += written;
  notify_line("UPLOAD_PROGRESS received=" + String(upload_received_size_) +
              "&size=" + String(upload_expected_size_));
}

void BleControl::handle_upload_end() {
  if (!upload_active_ || !upload_file_) {
    notify_line("ERR upload_not_active");
    return;
  }

  upload_file_.close();
  upload_active_ = false;
  if (upload_received_size_ != upload_expected_size_) {
    SPIFFS.remove(String("/gif/") + upload_filename_);
    notify_line("ERR upload_size_mismatch");
    upload_filename_ = "";
    upload_expected_size_ = 0;
    upload_received_size_ = 0;
    return;
  }

  auto& config = Configure::instance();
  config.gif_filename = upload_filename_;
  config.save();
  if (reload_callback_ != nullptr) {
    reload_callback_();
  }
  notify_line("UPLOAD_DONE name=" + upload_filename_ +
              "&size=" + String(upload_received_size_));
  upload_filename_ = "";
  upload_expected_size_ = 0;
  upload_received_size_ = 0;
  notify_state();
}

void BleControl::handle_upload_abort() {
  if (upload_file_) {
    upload_file_.close();
  }
  if (upload_active_ && upload_filename_.length() > 0) {
    SPIFFS.remove(String("/gif/") + upload_filename_);
  }
  upload_active_ = false;
  upload_filename_ = "";
  upload_expected_size_ = 0;
  upload_received_size_ = 0;
}

void BleControl::handle_gif_list_request() {
  File root = SPIFFS.open("/");
  if (!root) {
    notify_line("ERR gifs_dir_missing");
    return;
  }

  notify_line("GIFS_BEGIN");
  size_t count = 0;
  File file = root.openNextFile();
  while (file) {
    String path = file.name();
    String name = path;
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) {
      name = name.substring(slash + 1);
    }
    String lower_name = name;
    lower_name.toLowerCase();
    const bool in_gif_dir = path.startsWith("/gif/") || path.startsWith("gif/");
    if (!file.isDirectory() && in_gif_dir && lower_name.endsWith(".gif")) {
      notify_line("GIF file=" + url_encode(name));
      count++;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  notify_line("GIFS_END count=" + String(count));
}

void BleControl::handle_song_list_request() {
  File index = SPIFFS.open(kLyricIndexFile, "r");
  if (!index) {
    notify_line("ERR songs_index_missing");
    return;
  }

  notify_line("SONGS_BEGIN");
  size_t count = 0;
  while (index.available()) {
    String line = index.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }
    const int first_tab = line.indexOf('\t');
    const int second_tab = line.indexOf('\t', first_tab + 1);
    const int third_tab = line.indexOf('\t', second_tab + 1);
    if (first_tab < 0 || second_tab < 0 || third_tab < 0) {
      continue;
    }
    const String filename = line.substring(0, first_tab);
    const String title = line.substring(third_tab + 1);
    notify_line("SONG file=" + url_encode(filename) + "&title=" + url_encode(title));
    count++;
  }
  index.close();
  notify_line("SONGS_END count=" + String(count));
}

void BleControl::handle_lyric_request(const String& args) {
  const String filename = arg_value(args, "file");
  if (filename.length() == 0 || filename == "-") {
    notify_line("ERR lyric_bad_file");
    return;
  }

  String text;
  if (!LyricPlayer::instance().get_song_text(filename, text)) {
    notify_line("ERR lyric_read_failed file=" + url_encode(filename));
    return;
  }

  notify_line("LYRIC_BEGIN file=" + url_encode(filename));
  int pos = 0;
  while (pos < text.length()) {
    int next_newline = text.indexOf('\n', pos);
    if (next_newline < 0) {
      next_newline = text.length();
    }
    String line = text.substring(pos, next_newline);
    line.trim();
    if (line.length() > 0) {
      notify_line("LYRIC_TEXT line=" + url_encode(line));
    }
    pos = next_newline + 1;
  }
  notify_line("LYRIC_END file=" + url_encode(filename));
}

void BleControl::notify_state() {
  notify_line(build_state_line());
}

void BleControl::notify_line(const String& line) {
  if (state_characteristic_ == nullptr) {
    return;
  }

  String payload = line + "\n";
  int offset = 0;
  while (offset < payload.length()) {
    const int chunk_len = std::min<int>(kNotifyChunkSize, payload.length() - offset);
    const String chunk = payload.substring(offset, offset + chunk_len);
    state_characteristic_->setValue(chunk.c_str());
    state_characteristic_->notify();
    offset += chunk_len;
    delay(kNotifyChunkDelayMs);
  }
  state_characteristic_->setValue(line.c_str());
}

String BleControl::build_state_line() const {
  const auto& config = Configure::instance();
  String output = "STATE bright=";
  output += config.display_bright;
  output += "&enable=";
  output += config.enable_display ? "1" : "0";
  output += "&mode=";
  output += config.display_mode;
  output += "&gif=";
  output += config.gif_filename.length() ? config.gif_filename : "-";
  output += "&song=";
  output += config.lyric_filename.length() ? config.lyric_filename : "-";
  output += "&progress=";
  output += LyricPlayer::instance().current_progress_ms();
  output += "&color=";
  output += config.lyric_color;
  output += "&nextColor=";
  output += config.lyric_next_color;
  output += "&y1=";
  output += config.lyric_y1;
  output += "&y2=";
  output += config.lyric_y2;
  output += "&fsUsed=";
  output += SPIFFS.usedBytes();
  output += "&fsTotal=";
  output += SPIFFS.totalBytes();
  output += "&fontReady=";
  output += LyricPlayer::instance().font_ready() ? "1" : "0";
  output += "&lyricReady=";
  output += LyricPlayer::instance().song_ready() ? "1" : "0";
  output += "&lyricLines=";
  output += LyricPlayer::instance().line_count();
  output += "&preview=";
  output += LyricPlayer::instance().preview_ready() ? "1" : "0";
  return output;
}

String BleControl::arg_value(const String& args, const char* key) {
  const String prefix = String(key) + "=";
  int start = 0;
  while (start < args.length()) {
    int end = args.indexOf('&', start);
    if (end < 0) {
      end = args.length();
    }
    String token = args.substring(start, end);
    if (token.startsWith(prefix)) {
      return url_decode(token.substring(prefix.length()));
    }
    start = end + 1;
  }
  return "";
}

String BleControl::sanitize_gif_filename(const String& value) {
  String output;
  output.reserve(value.length());
  for (int i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-') {
      output += c;
    } else if (c == ' ') {
      output += '_';
    }
  }
  if (!output.endsWith(".gif") && !output.endsWith(".GIF")) {
    output += ".gif";
  }
  if (output.length() > 48) {
    output = output.substring(output.length() - 48);
  }
  return output;
}

String BleControl::url_encode(const String& value) {
  String output;
  output.reserve(value.length());
  const char* hex = "0123456789ABCDEF";
  for (int i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      output += char(c);
    } else if (c == ' ') {
      output += '+';
    } else {
      output += '%';
      output += hex[c >> 4];
      output += hex[c & 0x0f];
    }
  }
  return output;
}

String BleControl::url_decode(const String& value) {
  String output;
  output.reserve(value.length());
  for (int i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '+') {
      output += ' ';
      continue;
    }
    if (c == '%' && i + 2 < value.length()) {
      char hex[3] = {value[i + 1], value[i + 2], '\0'};
      output += char(strtoul(hex, nullptr, 16));
      i += 2;
      continue;
    }
    output += c;
  }
  return output;
}

bool BleControl::base64_decode(const String& input, std::vector<uint8_t>& output) {
  output.clear();
  int value = 0;
  int bits = -8;
  for (int i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c == '=') {
      break;
    }
    const int decoded = base64_value(c);
    if (decoded < 0) {
      return false;
    }
    value = (value << 6) | decoded;
    bits += 6;
    if (bits >= 0) {
      output.push_back(uint8_t((value >> bits) & 0xff));
      bits -= 8;
    }
  }
  return true;
}

int BleControl::base64_value(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9') {
    return c - '0' + 52;
  }
  if (c == '+') {
    return 62;
  }
  if (c == '/') {
    return 63;
  }
  return -1;
}
