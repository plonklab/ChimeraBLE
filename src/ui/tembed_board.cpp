// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
#if defined(CHIMERA_BOARD_TEMBED)
#include "tembed_board.h"
#include <Wire.h>

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
#endif
