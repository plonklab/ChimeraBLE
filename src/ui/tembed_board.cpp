// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
#if defined(CHIMERA_BOARD_TEMBED)
#include "tembed_board.h"
#include <Wire.h>
#include <driver/i2s_std.h>

TEmbedBoard BOARD_INSTANCE;

namespace {
constexpr int     kI2cSda    = 8;
constexpr int     kI2cScl    = 18;
constexpr uint8_t kGaugeAddr = 0x55;  // BQ27220
constexpr uint8_t kChgAddr   = 0x6B;  // BQ25896

void i2cInit() {
  static bool up = false;
  if (up) return;
  Wire.begin(kI2cSda, kI2cScl);
  Wire.setTimeOut(20);  // ms; never hang the UI on a dead bus
  up = true;
}

// Read `n` (<=2) little-endian bytes from register `reg`. false on NACK/short read.
bool i2cRead(uint8_t addr, uint8_t reg, uint8_t n, uint16_t &out) {
  i2cInit();
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, (int)n) != n) return false;
  uint16_t v = Wire.read();
  if (n == 2) v |= (uint16_t)Wire.read() << 8;
  out = v;
  return true;
}
}  // namespace

// BQ27220 standard commands: 0x08 Voltage (mV), 0x0C Current (mA, signed),
// 0x2C StateOfCharge (%).
int TEmbedPower::getBatteryLevel() {
  uint16_t v;
  if (!i2cRead(kGaugeAddr, 0x2C, 2, v)) return 0;
  return v > 100 ? 100 : (int)v;
}

int TEmbedPower::getBatteryVoltage() {
  uint16_t v;
  return i2cRead(kGaugeAddr, 0x08, 2, v) ? (int)v : 0;
}

int TEmbedPower::getBatteryCurrent() {
  uint16_t v;
  return i2cRead(kGaugeAddr, 0x0C, 2, v) ? (int)(int16_t)v : 0;
}

// Charging source: BQ25896 REG0B CHRG_STAT (bits 4:3): 1=pre-charge,
// 2=fast charge => charging; 0=not charging, 3=done => false.
bool TEmbedPower::isCharging() {
  uint16_t r;
  if (!i2cRead(kChgAddr, 0x0B, 1, r)) return false;
  uint8_t st = (r >> 3) & 0x03;
  return st == 1 || st == 2;
}

// ---- Speaker: MAX98357A over I2S std, 16 kHz mono 16-bit, square wave ----
namespace {
constexpr int      kI2sBclk = 46, kI2sWs = 40, kI2sDout = 7;
constexpr uint32_t kRate    = 16000;
constexpr uint32_t kDmaDesc = 6, kDmaFrames = 240;  // ring = 1440 mono samples
i2s_chan_handle_t g_tx = nullptr;
bool     g_i2sUp = false;

bool i2sInit() {
  if (g_i2sUp) return true;
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  cc.dma_desc_num  = kDmaDesc;
  cc.dma_frame_num = kDmaFrames;
  cc.auto_clear    = true;  // emit silence (not the last buffer) on underrun
  if (i2s_new_channel(&cc, &g_tx, nullptr) != ESP_OK) return false;
  i2s_std_config_t sc = {};
  sc.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(kRate);
  sc.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                    I2S_SLOT_MODE_MONO);
  sc.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  sc.gpio_cfg.bclk = (gpio_num_t)kI2sBclk;
  sc.gpio_cfg.ws   = (gpio_num_t)kI2sWs;
  sc.gpio_cfg.dout = (gpio_num_t)kI2sDout;
  sc.gpio_cfg.din  = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(g_tx, &sc) != ESP_OK ||
      i2s_channel_enable(g_tx) != ESP_OK) {
    i2s_del_channel(g_tx);
    g_tx = nullptr;
    return false;
  }
  g_i2sUp = true;
  return true;
}

void i2sWrite(const int16_t *buf, size_t n) {
  size_t wrote = 0;
  i2s_channel_write(g_tx, buf, n * sizeof(int16_t), &wrote, 200);
}
}  // namespace

void TEmbedSpeaker::setVolume(uint8_t v) { vol_ = v; }

void TEmbedSpeaker::tone(float freqHz, uint32_t ms) {
  if (freqHz < 20.0f || ms == 0 || vol_ == 0) return;
  if (freqHz > kRate / 2.0f) freqHz = kRate / 2.0f;
  if (!i2sInit()) return;
  // 0-255 volume -> up to ~1/2 full scale (headroom; speaker is small).
  const int16_t amp = (int16_t)(((uint32_t)vol_ * 16000u) / 255u);
  const uint32_t total = (uint32_t)((uint64_t)kRate * ms / 1000);
  const float step = freqHz / (float)kRate;
  float phase = 0.0f;
  int16_t buf[256];
  for (uint32_t done = 0; done < total;) {
    uint32_t n = total - done;
    if (n > 256) n = 256;
    for (uint32_t i = 0; i < n; i++) {
      buf[i] = phase < 0.5f ? amp : (int16_t)-amp;
      phase += step;
      if (phase >= 1.0f) phase -= 1.0f;
    }
    i2sWrite(buf, n);
    done += n;
  }
}

// Immediate cut-off: disabling drops queued DMA; re-enable so tone() still works.
// auto_clear keeps the line silent whenever nothing is queued.
void TEmbedSpeaker::stop() {
  if (!g_i2sUp) return;
  i2s_channel_disable(g_tx);
  i2s_channel_enable(g_tx);
}

// ---- WS2812 status LEDs (IDF RMT TX, copy encoder; no external LED lib) ----
// Policy: Idle = calm dim blue, Scanning = pulsing cyan, Connected = solid green.
// State is read board-side (scanner::isScanning / connection::isConnected), so
// App.cpp is untouched. setState() overrides until the next tick() poll.
#if CHIMERA_LEDS
#include <driver/rmt_tx.h>
#include "scanner.h"
#include "connection.h"

namespace {
constexpr int      kLedCount = 8;
constexpr uint32_t kRmtHz    = 10000000;  // 10 MHz -> 100 ns/tick
// WS2812B: T0H 0.4us, T0L 0.85us, T1H 0.8us, T1L 0.45us (+/-150ns); reset >50us low.
constexpr uint16_t kT0H = 4, kT0L = 8, kT1H = 8, kT1L = 5;
rmt_channel_handle_t g_led = nullptr;
rmt_encoder_handle_t g_enc = nullptr;
bool g_ledUp = false;
tembed_leds::State g_state = tembed_leds::Idle;
uint8_t g_lastRgb[3] = {0xFF, 0xFF, 0xFF};
bool g_lastValid = false;

void ledShow(uint8_t r, uint8_t g, uint8_t b) {
  if (!g_ledUp) return;
  if (g_lastValid && g_lastRgb[0] == r && g_lastRgb[1] == g && g_lastRgb[2] == b) return;
  rmt_symbol_word_t sym[kLedCount * 24];
  const uint8_t grb[3] = {g, r, b};  // WS2812 wire order is GRB
  int n = 0;
  for (int p = 0; p < kLedCount; p++)
    for (int c = 0; c < 3; c++)
      for (int bit = 7; bit >= 0; bit--) {
        const bool one = (grb[c] >> bit) & 1;
        sym[n].level0    = 1;
        sym[n].duration0 = one ? kT1H : kT0H;
        sym[n].level1    = 0;
        sym[n].duration1 = one ? kT1L : kT0L;
        n++;
      }
  rmt_transmit_config_t tc = {};
  tc.loop_count = 0;
  if (rmt_transmit(g_led, g_enc, sym, sizeof(sym), &tc) != ESP_OK) return;
  rmt_tx_wait_all_done(g_led, 20);  // sym[] is on the stack; wait before returning
  g_lastRgb[0] = r; g_lastRgb[1] = g; g_lastRgb[2] = b;
  g_lastValid = true;
}
}  // namespace

void tembed_leds::begin() {
  if (g_ledUp) return;
  rmt_tx_channel_config_t cc = {};
  cc.gpio_num          = (gpio_num_t)tembed_pins::LED_DATA;
  cc.clk_src           = RMT_CLK_SRC_DEFAULT;
  cc.resolution_hz     = kRmtHz;
  cc.mem_block_symbols = 64;
  cc.trans_queue_depth = 2;
  if (rmt_new_tx_channel(&cc, &g_led) != ESP_OK) return;
  rmt_copy_encoder_config_t ec = {};
  if (rmt_new_copy_encoder(&ec, &g_enc) != ESP_OK || rmt_enable(g_led) != ESP_OK) {
    if (g_enc) { rmt_del_encoder(g_enc); g_enc = nullptr; }
    rmt_del_channel(g_led);
    g_led = nullptr;
    return;
  }
  g_ledUp = true;
  ledShow(0, 0, 8);
}

void tembed_leds::setState(State s) { g_state = s; }

void tembed_leds::tick() {
  if (!g_ledUp) return;
  static uint32_t lastMs = 0;
  const uint32_t now = millis();
  if (now - lastMs < 30) return;  // ~33 fps cap
  lastMs = now;
  g_state = connection::isConnected() ? Connected
          : scanner::isScanning()     ? Scanning
                                      : Idle;
  switch (g_state) {
    case Connected: ledShow(0, 40, 0); break;
    case Scanning: {
      // triangle wave, 1.2 s period, 4..60 cyan
      const uint32_t ph = now % 1200;
      const uint32_t t  = ph < 600 ? ph : 1200 - ph;
      const uint8_t  v  = 4 + (uint8_t)((56u * t) / 600u);
      ledShow(0, v / 2, v);
      break;
    }
    default: ledShow(0, 0, 8); break;
  }
}
#endif
#endif
