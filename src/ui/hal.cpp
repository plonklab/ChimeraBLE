// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Board HAL implementation. One translation unit, board-selected by the
// -DCHIMERA_BOARD_* flag. The Cardputer path is a pure pass-through to the
// M5Cardputer global so the existing target is behavior-identical; the CoreS3
// path (M5Unified + CardKB) is added in a later task.
#include "hal.h"

#if defined(CHIMERA_BOARD_CARDPUTER)

namespace hal {

void begin() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);   // true = enable the built-in matrix keyboard
}

void update() { M5Cardputer.update(); }

KeyState readKeyboard() {
  KeyState k;
  k.changed = M5Cardputer.Keyboard.isChange();
  k.pressed = M5Cardputer.Keyboard.isPressed();
  const auto ks = M5Cardputer.Keyboard.keysState();
  k.word  = ks.word;
  k.enter = ks.enter;
  k.del   = ks.del;
  return k;
}

}  // namespace hal

#elif defined(CHIMERA_BOARD_CORES3)

#include "cardkb.h"

namespace hal {

void begin() {
  auto cfg = M5.config();
  M5.begin(cfg);     // M5Unified auto-detects CoreS3: display + AXP2101 + speaker
  cardkb::begin();   // M5 Unit CardKB on Grove Port.A
}

void update() {
  M5.update();
  cardkb::poll();
}

KeyState readKeyboard() { return cardkb::read(); }

}  // namespace hal

#elif defined(CHIMERA_BOARD_TEMBED)

#include <Arduino.h>

namespace hal {

// Stub: display bring-up only. Real input (encoder/keys) is added later.
void begin() {
  pinMode(tembed_pins::PWR_EN, OUTPUT);
  digitalWrite(tembed_pins::PWR_EN, HIGH);   // power-latch / peripheral rail
  BOARD.Display.init();
  BOARD.Display.setBrightness(128);
}

void update() { BOARD.update(); }

KeyState readKeyboard() { return KeyState{}; }

}  // namespace hal

#else
#error "No board selected for hal.cpp (define CHIMERA_BOARD_CARDPUTER, CHIMERA_BOARD_CORES3 or CHIMERA_BOARD_TEMBED)"
#endif
