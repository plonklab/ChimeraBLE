// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Pure, hardware-free CardKB edge/debounce decoder. Fed one byte per poll
// (0 = no key read this poll; else the ASCII the CardKB returned). Turns a
// stream of raw bytes into key-down edges so the UI's blocking input loops see
// one event per physical press, not continuous motion while a key is held.
//
// Deliberately free of Arduino/M5 includes so it can be unit-tested on the host
// (see test/test_cardkb/). cardkb.cpp owns the I2C read and maps Decoded onto
// hal::KeyState.
#pragma once

#include <cstdint>

namespace cardkb {

struct Decoded {
  char key     = 0;      // ASCII of the key down this poll; 0 = none
  bool enter   = false;  // key == 0x0D (CR)
  bool del     = false;  // key == 0x08 (BS)
  bool changed = false;  // a key-DOWN edge happened this poll
  bool pressed = false;  // a key is currently down
};

// Edge/debounce state machine. A fresh (different, non-zero) byte raises
// changed+pressed for exactly one feed(); the same byte repeated (a held key /
// auto-repeat with no gap) does NOT re-raise changed; a 0 resets so the next
// non-zero re-raises.
class Decoder {
 public:
  Decoded feed(uint8_t byte);

 private:
  uint8_t last_ = 0;
};

inline Decoded Decoder::feed(uint8_t byte) {
  Decoded d;
  if (byte == 0) {        // nothing down this poll — reset so a re-press re-edges
    last_ = 0;
    return d;
  }
  d.key     = static_cast<char>(byte);
  d.pressed = true;
  d.enter   = (byte == 0x0D);   // CR
  d.del     = (byte == 0x08);   // BS
  d.changed = (byte != last_);  // a down-edge only when the key differs from last poll
  last_ = byte;
  return d;
}

}  // namespace cardkb
