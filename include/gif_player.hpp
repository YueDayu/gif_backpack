#pragma once

#include <functional>

#include <AnimatedGIF.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <SPIFFS.h>

class GifPlayer {
 public:
  static GifPlayer& instance();

  bool init(const std::function<void(GifPlayer*)>& on_gif_done=nullptr);
  void set_gif_file(const String& filename);
  void update();

 private:
  GifPlayer() = default;
  void setup_display();
  void setup_gif();
  void draw_lyric_diagnostic();

  static void* gif_open_file(const char* filename, int32_t* p_size);
  static void gif_close_file(void* p_handle);
  static int32_t gif_read_file(GIFFILE* p_file, uint8_t* p_buf, int32_t len);
  static int32_t gif_seek_file(GIFFILE* p_file, int32_t pos);
  static void gif_draw(GIFDRAW* p_draw);

  File gif_file_;
  String cur_filename_;
  bool opened_ = false;
  bool need_reopen_ = false;
  bool static_frame_dirty_ = true;
  uint32_t last_static_draw_ms_ = 0;
  uint8_t cur_brightness_;

  MatrixPanel_I2S_DMA* dma_display_ = nullptr;
  AnimatedGIF gif_;

  std::function<void(GifPlayer*)> on_gif_done_ = nullptr;
};
