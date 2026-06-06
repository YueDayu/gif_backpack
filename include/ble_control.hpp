#pragma once

#include <Arduino.h>

#include <functional>

class NimBLECharacteristic;

class BleControl {
 public:
  bool begin(std::function<void()> reload_callback);
  void notify_state();
  void append_command_fragment(const String& fragment);

 private:
  void handle_command(const String& command);
  void handle_set_command(const String& args);
  void notify_line(const String& line);
  String build_state_line() const;

  static String arg_value(const String& args, const char* key);
  static String url_decode(const String& value);

  std::function<void()> reload_callback_ = nullptr;
  NimBLECharacteristic* state_characteristic_ = nullptr;
  String command_buffer_;
};
