#pragma once

#include <Arduino.h>
#include <FS.h>

#include <functional>
#include <vector>

class NimBLECharacteristic;

class BleControl {
 public:
  bool begin(std::function<void()> reload_callback);
  void notify_state();
  void append_command_fragment(const String& fragment);

 private:
  void handle_command(const String& command);
  void handle_set_command(const String& args);
  void handle_upload_begin(const String& args);
  void handle_upload_data(const String& args);
  void handle_upload_end();
  void handle_upload_abort();
  void notify_line(const String& line);
  String build_state_line() const;

  static String arg_value(const String& args, const char* key);
  static String sanitize_gif_filename(const String& value);
  static String url_decode(const String& value);
  static bool base64_decode(const String& input, std::vector<uint8_t>& output);
  static int base64_value(char c);

  std::function<void()> reload_callback_ = nullptr;
  NimBLECharacteristic* state_characteristic_ = nullptr;
  String command_buffer_;
  File upload_file_;
  String upload_filename_;
  uint32_t upload_expected_size_ = 0;
  uint32_t upload_received_size_ = 0;
  bool upload_active_ = false;
};
