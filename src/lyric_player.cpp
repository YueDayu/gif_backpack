#include "lyric_player.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "configure.hpp"

namespace {
const char* kLyricBaseDir = "/lyrics";
const char* kFontFile = "/lyrics/font.bin";
const char* kCharsetFile = "/lyrics/charset.txt";
const char* kLyricPackFile = "/lyrics/lyrics.pack";
const char* kLyricIndexFile = "/lyrics/index.txt";
constexpr uint8_t kGlyphHeight = 16;
constexpr uint16_t kScreenWidth = 64;
constexpr uint16_t kScreenHeight = 64;
}  // namespace

LyricPlayer& LyricPlayer::instance() {
  static LyricPlayer player;
  return player;
}

bool LyricPlayer::init() {
  font_ready_ = load_font();
  set_song_file(Configure::instance().lyric_filename);
  return font_ready_;
}

void LyricPlayer::set_song_file(const String& filename) {
  if (filename == loaded_song_ && (filename.length() == 0 || song_ready_)) {
    return;
  }
  loaded_song_ = filename;
  song_ready_ = false;
  progress_base_ms_ = 0;
  progress_base_clock_ = millis();
  if (loaded_song_.length() == 0) {
    clear_song();
    return;
  }
  if (!load_song(loaded_song_)) {
    clear_song();
    return;
  }
  song_ready_ = true;
}

void LyricPlayer::set_preview_text(const String& text) {
  preview_text_ = text == "-" ? "" : text;
}

void LyricPlayer::set_progress(uint32_t progress_ms) {
  progress_base_ms_ = duration_ms_ == 0 ? progress_ms : std::min(progress_ms, duration_ms_);
  progress_base_clock_ = millis();
}

uint32_t LyricPlayer::current_progress_ms() const {
  if (duration_ms_ == 0) {
    return progress_base_ms_;
  }
  const uint32_t elapsed = millis() - progress_base_clock_;
  return std::min(progress_base_ms_ + elapsed, duration_ms_);
}

bool LyricPlayer::draw(MatrixPanel_I2S_DMA* display) {
  if (!display || !font_ready_) {
    return false;
  }
  if (lines_.empty()) {
    return draw_preview_line(display);
  }

  const uint32_t progress_ms = current_progress_ms();
  const int line_index = current_line_index(progress_ms);
  if (line_index < 0) {
    return draw_preview_line(display);
  }

  const auto& config = Configure::instance();
  const uint16_t current_color = parse_color(display, config.lyric_color);
  const uint16_t next_color = parse_color(display, config.lyric_next_color);
  const int y1 = std::max(0, std::min<int>(config.lyric_y1, kScreenHeight - kGlyphHeight));
  const int y2 = std::max(0, std::min<int>(config.lyric_y2, kScreenHeight - kGlyphHeight));
  const int first_line_x = scroll_x_for_line(line_index, progress_ms);

  const bool current_drawn =
      draw_glyph_line(display, lines_[line_index].text, first_line_x, y1, current_color);
  const bool future_drawn =
      draw_future_line(display, line_index, first_line_x, y2, current_color, next_color);
  return current_drawn || future_drawn;
}

bool LyricPlayer::draw_preview_line(MatrixPanel_I2S_DMA* display) const {
  if (preview_text_.length() == 0) {
    return false;
  }

  const auto& config = Configure::instance();
  const uint16_t current_color = parse_color(display, config.lyric_color);
  const uint16_t next_color = parse_color(display, config.lyric_next_color);
  const int y1 = std::max(0, std::min<int>(config.lyric_y1, kScreenHeight - kGlyphHeight));
  const int y2 = std::max(0, std::min<int>(config.lyric_y2, kScreenHeight - kGlyphHeight));
  const uint16_t width = text_width(preview_text_);
  if (width <= kScreenWidth) {
    return draw_glyph_line(display, preview_text_, (kScreenWidth - width) / 2, y1, current_color);
  }

  const bool first_drawn = draw_glyph_line(display, preview_text_, 0, y1, current_color);
  const bool second_drawn = draw_glyph_line(display, preview_text_, -kScreenWidth, y2, next_color);
  return first_drawn || second_drawn;
}

bool LyricPlayer::load_font() {
  File font_file = SPIFFS.open(kFontFile, "r");
  File charset_file = SPIFFS.open(kCharsetFile, "r");
  if (!font_file || !charset_file) {
    return false;
  }

  font_bytes_.resize(font_file.size());
  font_file.read(font_bytes_.data(), font_bytes_.size());
  String charset = charset_file.readString();
  font_file.close();
  charset_file.close();

  glyphs_.clear();

  int count_pos = 0;
  size_t glyph_count = 0;
  uint32_t expected_font_bytes = 0;
  uint32_t count_codepoint = 0;
  while (next_codepoint(charset, count_pos, count_codepoint)) {
    glyph_count++;
    expected_font_bytes += is_han(count_codepoint) ? 32 : 16;
  }
  if (expected_font_bytes > font_bytes_.size()) {
    return false;
  }
  glyphs_.reserve(glyph_count);

  int pos = 0;
  uint32_t offset = 0;
  uint32_t codepoint = 0;
  while (next_codepoint(charset, pos, codepoint)) {
    const uint8_t width = is_han(codepoint) ? 16 : 8;
    const uint8_t byte_len = (width * kGlyphHeight) / 8;
    if (offset + byte_len > font_bytes_.size()) {
      break;
    }
    Glyph glyph;
    glyph.codepoint = codepoint;
    glyph.offset = offset;
    glyph.width = width;
    glyphs_.push_back(glyph);
    offset += byte_len;
  }

  return !glyphs_.empty() && offset <= font_bytes_.size();
}

bool LyricPlayer::load_song(const String& filename) {
  if (!font_ready_) {
    return false;
  }

  String song_text;
  if (!read_song_text(filename, song_text)) {
    return false;
  }

  lines_.clear();
  int text_pos = 0;
  while (text_pos < song_text.length()) {
    int next_newline = song_text.indexOf('\n', text_pos);
    if (next_newline < 0) {
      next_newline = song_text.length();
    }
    String raw = song_text.substring(text_pos, next_newline);
    text_pos = next_newline + 1;
    raw.trim();
    if (raw.length() == 0) {
      continue;
    }

    std::vector<uint32_t> times;
    int search_from = 0;
    while (search_from < raw.length()) {
      const int left = raw.indexOf('[', search_from);
      if (left < 0) {
        break;
      }
      const int right = raw.indexOf(']', left + 1);
      if (right < 0) {
        break;
      }
      uint32_t time_ms = 0;
      if (parse_timestamp_ms(raw.substring(left + 1, right), time_ms)) {
        times.push_back(time_ms);
      }
      search_from = right + 1;
    }
    if (times.empty()) {
      continue;
    }

    String text = raw;
    while (true) {
      const int left = text.indexOf('[');
      const int right = left >= 0 ? text.indexOf(']', left + 1) : -1;
      if (left < 0 || right < 0) {
        break;
      }
      text.remove(left, right - left + 1);
    }
    text.trim();
    if (text.length() == 0) {
      continue;
    }

    for (uint32_t time_ms : times) {
      LyricLine line;
      line.time_ms = time_ms;
      line.text = text;
      line.width = text_width(text);
      lines_.push_back(line);
    }
  }

  std::sort(lines_.begin(), lines_.end(), [](const LyricLine& a, const LyricLine& b) {
    return a.time_ms < b.time_ms;
  });

  duration_ms_ = lines_.empty() ? 0 : lines_.back().time_ms + 5000;
  set_progress(0);
  return !lines_.empty();
}

bool LyricPlayer::read_song_text(const String& filename, String& text) const {
  String direct_path = String(kLyricBaseDir) + "/" + filename;
  File direct = SPIFFS.open(direct_path, "r");
  if (direct) {
    text = direct.readString();
    direct.close();
    return text.length() > 0;
  }

  File index = SPIFFS.open(kLyricIndexFile, "r");
  if (!index) {
    return false;
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
    return false;
  }

  File pack = SPIFFS.open(kLyricPackFile, "r");
  if (!pack) {
    return false;
  }
  if (!pack.seek(offset)) {
    pack.close();
    return false;
  }

  std::vector<char> buffer(length + 1);
  const size_t read_bytes = pack.readBytes(buffer.data(), length);
  pack.close();
  if (read_bytes != length) {
    return false;
  }
  buffer[length] = '\0';
  text = String(buffer.data());
  return text.length() > 0;
}

void LyricPlayer::clear_song() {
  lines_.clear();
  duration_ms_ = 0;
  progress_base_ms_ = 0;
  progress_base_clock_ = millis();
  song_ready_ = false;
}

uint16_t LyricPlayer::text_width(const String& text) const {
  uint16_t width = 0;
  int pos = 0;
  uint32_t codepoint = 0;
  while (next_codepoint(text, pos, codepoint)) {
    const Glyph* glyph = find_glyph(codepoint);
    width += glyph ? glyph->width : 8;
  }
  return width;
}

int LyricPlayer::current_line_index(uint32_t progress_ms) const {
  if (lines_.empty()) {
    return -1;
  }
  int index = 0;
  for (size_t i = 0; i < lines_.size(); ++i) {
    if (lines_[i].time_ms <= progress_ms) {
      index = static_cast<int>(i);
    } else {
      break;
    }
  }
  return index;
}

int LyricPlayer::scroll_x_for_line(int line_index, uint32_t progress_ms) const {
  if (line_index < 0 || line_index >= static_cast<int>(lines_.size())) {
    return 0;
  }

  const LyricLine& line = lines_[line_index];
  if (line.width <= kScreenWidth) {
    return (kScreenWidth - line.width) / 2;
  }

  const uint32_t start_ms = line.time_ms;
  const uint32_t end_ms = line_index + 1 < static_cast<int>(lines_.size())
                              ? lines_[line_index + 1].time_ms
                              : duration_ms_;
  const uint32_t duration = std::max<uint32_t>(1500, end_ms - start_ms);
  if (progress_ms <= start_ms) {
    return 0;
  }
  const float progress =
      std::max(0.0f, std::min(1.0f, float(progress_ms - start_ms) / float(duration)));
  const int overflow = line.width - kScreenWidth;
  return -int(roundf(float(overflow) * progress));
}

bool LyricPlayer::draw_glyph_line(MatrixPanel_I2S_DMA* display,
                                  const String& text,
                                  int start_x,
                                  int start_y,
                                  uint16_t color) const {
  bool drawn = false;
  int cursor_x = start_x;
  int pos = 0;
  uint32_t codepoint = 0;
  while (next_codepoint(text, pos, codepoint)) {
    const Glyph* glyph = find_glyph(codepoint);
    const uint8_t glyph_width = glyph ? glyph->width : 8;
    if (glyph && cursor_x < kScreenWidth && cursor_x + glyph_width > 0) {
      for (uint8_t y = 0; y < kGlyphHeight; ++y) {
        const int py = start_y + y;
        if (py < 0 || py >= kScreenHeight) {
          continue;
        }
        for (uint8_t x = 0; x < glyph->width; ++x) {
          const int px = cursor_x + x;
          if (px < 0 || px >= kScreenWidth) {
            continue;
          }
          if (glyph_bit(*glyph, x, y)) {
            display->drawPixel(px, py, color);
            drawn = true;
          }
        }
      }
    }
    cursor_x += glyph_width;
  }
  return drawn;
}

bool LyricPlayer::draw_future_line(MatrixPanel_I2S_DMA* display,
                                   int line_index,
                                   int first_line_x,
                                   int y,
                                   uint16_t current_color,
                                   uint16_t next_color) const {
  const LyricLine& line = lines_[line_index];
  const int second_line_x = first_line_x - kScreenWidth;
  bool drawn = draw_glyph_line(display, line.text, second_line_x, y, current_color);

  if (line_index + 1 >= static_cast<int>(lines_.size())) {
    return drawn;
  }

  const int current_tail_right = second_line_x + line.width;
  const int next_start_x = std::max(0, current_tail_right + 4);
  if (next_start_x < kScreenWidth) {
    drawn = draw_glyph_line(display, lines_[line_index + 1].text, next_start_x, y, next_color) ||
            drawn;
  }
  return drawn;
}

const LyricPlayer::Glyph* LyricPlayer::find_glyph(uint32_t codepoint) const {
  int left = 0;
  int right = static_cast<int>(glyphs_.size()) - 1;
  while (left <= right) {
    const int mid = left + (right - left) / 2;
    if (glyphs_[mid].codepoint == codepoint) {
      return &glyphs_[mid];
    }
    if (glyphs_[mid].codepoint < codepoint) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  return nullptr;
}

bool LyricPlayer::glyph_bit(const Glyph& glyph, uint8_t x, uint8_t y) const {
  const uint32_t bit_index = uint32_t(y) * glyph.width + x;
  const uint32_t byte_offset = glyph.offset + (bit_index >> 3);
  if (byte_offset >= font_bytes_.size()) {
    return false;
  }
  return (font_bytes_[byte_offset] & (0x80 >> (bit_index & 7))) != 0;
}

uint16_t LyricPlayer::parse_color(MatrixPanel_I2S_DMA* display, const String& color) const {
  String hex = color;
  hex.trim();
  if (hex.startsWith("#")) {
    hex.remove(0, 1);
  }
  if (hex.length() != 6) {
    return display->color565(255, 204, 51);
  }
  const uint32_t rgb = strtoul(hex.c_str(), nullptr, 16);
  return display->color565((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

bool LyricPlayer::next_codepoint(const String& text, int& pos, uint32_t& codepoint) {
  const int len = text.length();
  if (pos >= len) {
    return false;
  }

  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text.c_str());
  const uint8_t first = bytes[pos];
  if (first < 0x80) {
    codepoint = first;
    pos += 1;
    return true;
  }

  uint8_t count = 0;
  if ((first & 0xe0) == 0xc0) {
    codepoint = first & 0x1f;
    count = 2;
  } else if ((first & 0xf0) == 0xe0) {
    codepoint = first & 0x0f;
    count = 3;
  } else if ((first & 0xf8) == 0xf0) {
    codepoint = first & 0x07;
    count = 4;
  } else {
    codepoint = '?';
    pos += 1;
    return true;
  }

  if (pos + count > len) {
    pos = len;
    codepoint = '?';
    return true;
  }

  for (uint8_t i = 1; i < count; ++i) {
    codepoint = (codepoint << 6) | (bytes[pos + i] & 0x3f);
  }
  pos += count;
  return true;
}

bool LyricPlayer::is_han(uint32_t codepoint) {
  return (codepoint >= 0x3400 && codepoint <= 0x4dbf) ||
         (codepoint >= 0x4e00 && codepoint <= 0x9fff) ||
         (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
         (codepoint >= 0x20000 && codepoint <= 0x2a6df) ||
         (codepoint >= 0x2a700 && codepoint <= 0x2b73f) ||
         (codepoint >= 0x2b740 && codepoint <= 0x2b81f) ||
         (codepoint >= 0x2b820 && codepoint <= 0x2ceaf) ||
         (codepoint >= 0x2ceb0 && codepoint <= 0x2ebef) ||
         (codepoint >= 0x30000 && codepoint <= 0x3134f) ||
         (codepoint >= 0x31350 && codepoint <= 0x323af);
}

bool LyricPlayer::parse_timestamp_ms(const String& token, uint32_t& time_ms) {
  const int colon = token.indexOf(':');
  if (colon <= 0) {
    return false;
  }

  const int dot = token.indexOf('.', colon + 1);
  const int second_end = dot >= 0 ? dot : token.indexOf(':', colon + 1);
  const int end = second_end >= 0 ? second_end : token.length();
  const int minute = token.substring(0, colon).toInt();
  const int second = token.substring(colon + 1, end).toInt();
  if (second < 0 || second > 59) {
    return false;
  }

  int ms = 0;
  if (dot >= 0 && dot + 1 < token.length()) {
    String fraction = token.substring(dot + 1);
    if (fraction.length() == 1) {
      ms = fraction.toInt() * 100;
    } else if (fraction.length() == 2) {
      ms = fraction.toInt() * 10;
    } else {
      ms = fraction.substring(0, 3).toInt();
    }
  }
  time_ms = uint32_t(minute) * 60000 + uint32_t(second) * 1000 + uint32_t(ms);
  return true;
}
