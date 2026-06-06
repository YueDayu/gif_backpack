#include "gif_player.hpp"

#include <algorithm>

#include "configure.hpp"
#include "lyric_player.hpp"

const int panel_res_x = 64;
const int panel_res_y = 64;
const int panel_chain = 1;
const String gif_basedir = "/gif";

uint8_t scaled_brightness(uint8_t percent) {
  if (percent == 0) {
    return 24;
  }
  const int value = int(float(percent) / 100.0f * 233.0f);
  return uint8_t(std::max(24, std::min(233, value)));
}

GifPlayer& GifPlayer::instance() {
  static GifPlayer player;
  return player;
}

bool GifPlayer::init(const std::function<void(GifPlayer*)>& on_gif_done) {
  on_gif_done_ = on_gif_done;
  setup_display();
  setup_gif();
  return true;
}

void GifPlayer::set_gif_file(const String& filename) {
  if (filename != cur_filename_) {
    cur_filename_ = filename;
    need_reopen_ = true;
    static_frame_dirty_ = true;
  }
}

void GifPlayer::update() {
  constexpr uint32_t kStaticFrameIntervalMs = 33;
  const auto& config = Configure::instance();
  const bool lyric_mode = config.display_mode == "lyrics";
  if (config.display_bright != cur_brightness_) {
    cur_brightness_ = config.display_bright;
    dma_display_->setBrightness8(scaled_brightness(cur_brightness_));
    static_frame_dirty_ = true;
  }
  if (!config.enable_display) {
    dma_display_->fillScreen(dma_display_->color565(0, 0, 0));
    static_frame_dirty_ = true;
    return;
  }

  if (lyric_mode) {
    if (opened_) {
      gif_.close();
      opened_ = false;
    }
    const uint32_t now = millis();
    if (!static_frame_dirty_ && now - last_static_draw_ms_ < kStaticFrameIntervalMs) {
      return;
    }
    dma_display_->fillScreen(dma_display_->color565(0, 0, 0));
    if (!LyricPlayer::instance().draw(dma_display_)) {
      draw_lyric_diagnostic();
    }
    static_frame_dirty_ = false;
    last_static_draw_ms_ = now;
    return;
  }

  if (cur_filename_.length() == 0) {
    if (opened_) {
      gif_.close();
      opened_ = false;
    }
    if (static_frame_dirty_) {
      dma_display_->fillScreen(dma_display_->color565(0, 0, 0));
      static_frame_dirty_ = false;
    }
    return;
  }

  if (!opened_ && !need_reopen_) {
    need_reopen_ = true;
  }

  if (need_reopen_) {
    if (opened_) {
      gif_.close();
    }
    opened_ = false;
    if (cur_filename_.length() > 0) {
      String filename = gif_basedir + '/' + cur_filename_;
      opened_ = gif_.open(filename.c_str(),
                          &gif_open_file,
                          &gif_close_file,
                          &gif_read_file,
                          &gif_seek_file,
                          &gif_draw);
    }
    need_reopen_ = false;
  } else {
    if (opened_ && !gif_.playFrame(true, nullptr)) {
      if (on_gif_done_ != nullptr) {
        on_gif_done_(this);
      }
      if (!need_reopen_) {
        gif_.reset();
      }
    }
  }

  if (!opened_) {
    if (static_frame_dirty_) {
      dma_display_->fillScreen(dma_display_->color565(0, 0, 0));
      static_frame_dirty_ = false;
    }
    return;
  }
}

void GifPlayer::setup_display() {
  HUB75_I2S_CFG mxconfig(panel_res_x,  // module width
                         panel_res_y,  // module height
                         panel_chain   // Chain length
  );
  mxconfig.gpio.e = 18;
  mxconfig.clkphase = false;
  dma_display_ = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display_->begin();
  cur_brightness_ = Configure::instance().display_bright;
  dma_display_->setBrightness8(scaled_brightness(cur_brightness_));
  dma_display_->clearScreen();
  dma_display_->setRotation(2);
  dma_display_->fillScreen(dma_display_->color565(0, 0, 0));
}

void GifPlayer::setup_gif() { gif_.begin(LITTLE_ENDIAN_PIXELS); }

void GifPlayer::draw_lyric_diagnostic() {
  const auto& lyric = LyricPlayer::instance();
  uint16_t color = dma_display_->color565(0, 180, 255);
  if (!lyric.font_ready()) {
    color = dma_display_->color565(255, 0, 0);
  } else if (!lyric.song_ready() || lyric.line_count() == 0) {
    color = dma_display_->color565(255, 180, 0);
  }
  dma_display_->drawRect(0, 0, 64, 64, color);
  dma_display_->drawRect(2, 2, 60, 60, color);
  dma_display_->fillRect(6, 6, 10, 10, color);
  dma_display_->fillRect(24, 6, 10, 10, dma_display_->color565(255, 255, 255));
  dma_display_->fillRect(42, 6, 10, 10, color);
  dma_display_->drawLine(8, 28, 55, 28, color);
  dma_display_->drawLine(8, 40, 55, 40, color);
}

void* GifPlayer::gif_open_file(const char* filename, int32_t* p_size) {
  instance().gif_file_ = SPIFFS.open(filename);
  if (instance().gif_file_) {
    *p_size = instance().gif_file_.size();
    return (void*)&(instance().gif_file_);
  }
  return nullptr;
}

void GifPlayer::gif_close_file(void* p_handle) {
  File* f = static_cast<File*>(p_handle);
  if (f != nullptr) {
    f->close();
  }
}

int32_t GifPlayer::gif_read_file(GIFFILE* p_file, uint8_t* p_buf, int32_t len) {
  int32_t read_bytes = len;
  File* f = static_cast<File*>(p_file->fHandle);
  if ((p_file->iSize - p_file->iPos) < len) {
    read_bytes = p_file->iSize - p_file->iPos - 1;  // <-- ugly work-around
  }
  if (read_bytes <= 0) {
    return 0;
  }
  read_bytes = (int32_t)f->read(p_buf, read_bytes);
  p_file->iPos = f->position();
  return read_bytes;
}

int32_t GifPlayer::gif_seek_file(GIFFILE* p_file, int32_t pos) {
  File* f = static_cast<File*>(p_file->fHandle);
  f->seek(pos);
  p_file->iPos = (int32_t)f->position();
  return p_file->iPos;
}

void GifPlayer::gif_draw(GIFDRAW* p_draw) {
  uint8_t* s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  usPalette = p_draw->pPalette;
  y = p_draw->iY + p_draw->y;

  s = p_draw->pPixels;
  if (p_draw->ucDisposalMethod == 2) {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == p_draw->ucTransparent) {
        s[x] = p_draw->ucBackground;
      }
    }
    p_draw->ucHasTransparency = 0;
  }
  if (p_draw->ucHasTransparency) {
    uint8_t *pEnd, c, ucTransparent = p_draw->ucTransparent;
    int x, iCount;
    pEnd = s + p_draw->iWidth;
    x = 0;
    iCount = 0;  // count non-transparent pixels
    while (x < p_draw->iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          s--;    // back up to treat it like transparent
        } else {  // opaque
          *d++ = usPalette[c];
          iCount++;
        }
      }  // while looking for opaque pixels
      if (iCount) {  // any opaque pixels?
        for (int xOffset = 0; xOffset < iCount; xOffset++) {
          (instance().dma_display_)->drawPixel(x + xOffset + p_draw->iX, y, usTemp[xOffset]);
        }
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          iCount++;
        } else {
          s--;
        }
      }
      if (iCount) {
        x += iCount;  // skip these
        iCount = 0;
      }
    }
  } else {
    s = p_draw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < p_draw->iWidth; x++) {
      (instance().dma_display_)->drawPixel(x + p_draw->iX, y, usPalette[*s++]);
    }
  }
}
