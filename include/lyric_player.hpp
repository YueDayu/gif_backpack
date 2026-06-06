#pragma once

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <SPIFFS.h>

#include <vector>

class LyricPlayer {
 public:
  static LyricPlayer& instance();

  bool init();
  void set_song_file(const String& filename);
  void set_preview_text(const String& text);
  bool get_song_text(const String& filename, String& text) const;
  void set_progress(uint32_t progress_ms);
  uint32_t current_progress_ms() const;
  bool draw(MatrixPanel_I2S_DMA* display);
  bool font_ready() const { return font_ready_; }
  bool song_ready() const { return song_ready_; }
  bool preview_ready() const { return preview_text_.length() > 0; }
  size_t line_count() const { return lines_.size(); }

 private:
  struct Glyph {
    uint32_t codepoint = 0;
    uint32_t offset = 0;
    uint8_t width = 8;
  };

  struct LyricLine {
    uint32_t time_ms = 0;
    String text;
    uint16_t width = 0;
  };

  LyricPlayer() = default;

  bool load_font();
  bool load_song(const String& filename);
  bool read_song_text(const String& filename, String& text) const;
  void clear_song();

  uint16_t text_width(const String& text) const;
  int current_line_index(uint32_t progress_ms) const;
  int scroll_x_for_line(int line_index, uint32_t progress_ms) const;
  bool draw_preview_line(MatrixPanel_I2S_DMA* display) const;
  bool draw_glyph_line(MatrixPanel_I2S_DMA* display,
                       const String& text,
                       int start_x,
                       int start_y,
                       uint16_t color) const;
  bool draw_future_line(MatrixPanel_I2S_DMA* display,
                        int line_index,
                        int first_line_x,
                        int y,
                        uint16_t current_color,
                        uint16_t next_color) const;

  bool find_glyph(uint32_t codepoint, Glyph& glyph) const;
  bool glyph_bit(const Glyph& glyph, uint8_t x, uint8_t y) const;
  uint16_t parse_color(MatrixPanel_I2S_DMA* display, const String& color) const;

  static bool next_codepoint(const String& text, int& pos, uint32_t& codepoint);
  static bool is_han(uint32_t codepoint);
  static bool parse_timestamp_ms(const String& token, uint32_t& time_ms);

  std::vector<uint8_t> font_bytes_;
  std::vector<uint32_t> glyph_codepoints_;
  std::vector<uint16_t> glyph_offset_units_;
  std::vector<LyricLine> lines_;

  String loaded_song_;
  String preview_text_;
  uint32_t duration_ms_ = 0;
  uint32_t progress_base_ms_ = 0;
  uint32_t progress_base_clock_ = 0;
  bool font_ready_ = false;
  bool song_ready_ = false;
};
