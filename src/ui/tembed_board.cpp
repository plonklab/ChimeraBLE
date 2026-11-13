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
i2s_chan_handle_t g_tx = nullptr;
bool     g_i2sUp = false;

bool i2sInit() {
  if (g_i2sUp) return true;
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
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

void TEmbedSpeaker::stop() {
  if (!g_i2sUp) return;
  int16_t z[64] = {0};
  i2sWrite(z, 64);
}
#endif
