// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// LilyGo T-Embed CC1101 Plus board shim. Provides the BOARD-shaped object the
// UI expects (Display / Speaker / Power / update()) on top of plain LovyanGFX,
// so UI.cpp / App.cpp stay board-agnostic. Display only in this revision;
// Speaker/Power are stubs filled by later tasks. The LGFX class and the single
// BOARD_INSTANCE live in tembed_board.cpp (one translation unit).
#pragma once

#include <cstdint>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// The UI declares `M5Canvas canvas_;` — on this board that is a plain sprite.
using M5Canvas = LGFX_Sprite;

// Pins (T-Embed CC1101 Plus). Display SPI bus is shared with the SD card.
namespace tembed_pins {
static constexpr int TFT_SCLK = 11;
static constexpr int TFT_MOSI = 9;
static constexpr int TFT_MISO = 10;
static constexpr int TFT_CS   = 41;
static constexpr int TFT_DC   = 16;
static constexpr int TFT_RST  = -1;   // IO40 is the audio word-clock; Task 7 resolves
static constexpr int TFT_BL   = 21;
static constexpr int PWR_EN   = 15;
static constexpr int LED_DATA = 14;   // 8x WS2812
}  // namespace tembed_pins

// ST7789, 170x320 native, used as 320x170 landscape.
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI      bus_;
  lgfx::Light_PWM    light_;

 public:
  LGFX() {
    {
      auto cfg = bus_.config();
      cfg.spi_host    = SPI3_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = tembed_pins::TFT_SCLK;
      cfg.pin_mosi    = tembed_pins::TFT_MOSI;
      cfg.pin_miso    = -1;   // write-only; MISO (IO10) is left to the SD driver
      cfg.pin_dc      = tembed_pins::TFT_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs           = tembed_pins::TFT_CS;
      cfg.pin_rst          = tembed_pins::TFT_RST;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 170;
      cfg.panel_height     = 320;
      cfg.offset_x         = 35;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;   // SD card shares SCK/MOSI
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl      = tembed_pins::TFT_BL;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }
};

// MAX98357A I2S tone generator (BCLK=46, WS=40, DOUT=7). Defined in
// tembed_board.cpp. tone() is a short blocking write; I2S is lazily started.
struct TEmbedSpeaker {
  void setVolume(uint8_t v);
  void tone(float freqHz, uint32_t ms);
  void stop();
private:
  uint8_t vol_ = 128;
};

// Power/battery over the shared I2C bus (SDA=8, SCL=18): BQ27220 fuel gauge
// (0x55) for SoC/voltage/current, BQ25896 charger (0x6B) for charge status.
// Defined in tembed_board.cpp. Failed reads return 0/false.
struct TEmbedPower {
  int  getBatteryLevel();
  bool isCharging();
  int  getBatteryVoltage();
  int  getBatteryCurrent();
};

// WS2812 x8 status LEDs (GPIO14). Compiled only when CHIMERA_LEDS != 0;
// otherwise begin/setState/tick are empty inline no-ops (zero RMT code linked).
#ifndef CHIMERA_LEDS
#define CHIMERA_LEDS 1
#endif
namespace tembed_leds {
enum State : uint8_t { Idle, Scanning, Connected };
#if CHIMERA_LEDS
void begin();
void setState(State s);
void tick();   // polls scanner/connection state + advances the pulse
#else
inline void begin() {}
inline void setState(State) {}
inline void tick() {}
#endif
}  // namespace tembed_leds

struct TEmbedBoard {
  LGFX          Display;
  TEmbedSpeaker Speaker;
  TEmbedPower   Power;
  void update() { tembed_leds::tick(); }
};

extern TEmbedBoard BOARD_INSTANCE;
