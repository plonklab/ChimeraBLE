// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Board-neutral hardware abstraction for the on-device UI. board.h binds BOARD
// (the M5 global) + constants; this header covers the one piece that genuinely
// differs between boards: the keyboard. On the Cardputer it wraps the built-in
// matrix keyboard; on CoreS3 it reads an M5 Unit CardKB over I2C (see cardkb.*).
#pragma once

#include <vector>
#include "board.h"

namespace hal {

// Board-neutral mirror of the fields the UI reads off a key event. `changed`
// and `pressed` replace M5Cardputer.Keyboard.isChange()/isPressed(); word/enter/
// del mirror Keyboard_Class::KeysState.
struct KeyState {
  std::vector<char> word;    // characters down this poll
  bool enter   = false;
  bool del     = false;
  bool changed = false;      // a key-state transition happened this poll
  bool pressed = false;      // a key is currently down
};

void     begin();            // board bring-up (owns M5.config(); + CardKB on CoreS3)
void     update();           // per-loop pump (BOARD.update() + CardKB poll on CoreS3)
KeyState readKeyboard();     // call right after update(); the current key edge/state

}  // namespace hal
