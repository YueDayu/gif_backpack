#include "ble_control.hpp"

#include <NimBLEDevice.h>
#include <Preferences.h>

#include <algorithm>

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
  if (command.startsWith("SET ")) {
    handle_set_command(command.substring(4));
    return;
  }
  notify_line("ERR unknown_command");
}

void BleControl::handle_set_command(const String& args) {
  auto& config = Configure::instance();
  bool needs_save = false;
  bool needs_reload = false;
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

  value = arg_value(args, "gif");
  if (value.length() > 0 && value != config.gif_filename) {
    config.gif_filename = value == "-" ? "" : value;
    needs_save = true;
    needs_reload = true;
  }

  value = arg_value(args, "song");
  if (value.length() > 0 && value != config.lyric_filename) {
    config.lyric_filename = value == "-" ? "" : value;
    requested_progress = 0;
    has_progress = true;
    needs_save = true;
    needs_reload = true;
  }

  value = arg_value(args, "color");
  if (value.length() > 0) {
    value.trim();
    if (value.length() == 7 && value.startsWith("#")) {
      config.lyric_color = value;
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
  if (has_progress) {
    LyricPlayer::instance().set_progress(requested_progress);
  }

  notify_state();
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
  output += "&gif=";
  output += config.gif_filename.length() ? config.gif_filename : "-";
  output += "&song=";
  output += config.lyric_filename.length() ? config.lyric_filename : "-";
  output += "&progress=";
  output += LyricPlayer::instance().current_progress_ms();
  output += "&color=";
  output += config.lyric_color;
  output += "&y1=";
  output += config.lyric_y1;
  output += "&y2=";
  output += config.lyric_y2;
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
