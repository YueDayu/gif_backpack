#pragma once

#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include <functional>
#include <vector>

class NimBLECharacteristic;

class BleControl {
 public:
  bool begin(std::function<void()> reload_callback);
  void update();
  void notify_state();
  void append_command_fragment(const String& fragment);

 private:
  void enqueue_command(const String& command);
  void handle_command(const String& command);
  void handle_set_command(const String& args);
  void handle_upload_begin(const String& args);
  void handle_upload_data(const String& args);
  void handle_upload_end();
  void handle_upload_abort();
  void handle_gif_list_request();
  void handle_song_list_request();
  void handle_lyric_request(const String& args);
  void notify_line(const String& line);
  void flush_batch(String& batch);
  String build_state_line() const;

  static String arg_value(const String& args, const char* key);
  static String sanitize_gif_filename(const String& value);
  static String url_encode(const String& value);
  static String url_decode(const String& value);
  static bool base64_decode(const String& input, std::vector<uint8_t>& output);
  static int base64_value(char c);

  std::function<void()> reload_callback_ = nullptr;
  NimBLECharacteristic* state_characteristic_ = nullptr;
  portMUX_TYPE command_queue_mux_ = portMUX_INITIALIZER_UNLOCKED;
  String command_buffer_;
  std::vector<String> pending_commands_;
  File upload_file_;
  String upload_filename_;
  uint32_t upload_expected_size_ = 0;
  uint32_t upload_received_size_ = 0;
  bool upload_active_ = false;
};
