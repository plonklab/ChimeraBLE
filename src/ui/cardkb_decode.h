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

// The CardKB v1.1 resolves letters/Shift/Sym/Fn on-device and returns plain
// ASCII, but its dedicated arrow cluster + ESC key emit these fixed non-ASCII
// bytes instead (standard across the M5 CardKB firmware). If a key turns out to
// disagree on real hardware, correct it here -- the decoder is the one place
// these raw codes are named. The decoder translates each into the Cardputer nav
// character the shared UI already handles (see the switch in feed()).
constexpr uint8_t kCardKbLeft  = 0xB4;   // -> ',' (back)
constexpr uint8_t kCardKbUp    = 0xB5;   // -> ';' (up)
constexpr uint8_t kCardKbDown  = 0xB6;   // -> '.' (down)
constexpr uint8_t kCardKbRight = 0xB7;   // -> '/' (select/activate)
constexpr uint8_t kCardKbEsc   = 0x1B;   // -> '`' (ESC action menu)

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
  d.pressed = true;
  d.enter   = (byte == 0x0D);   // CR
  d.del     = (byte == 0x08);   // BS
  d.changed = (byte != last_);  // a down-edge only when the key differs from last poll
  last_ = byte;                 // edge tracks the RAW byte, so a held arrow doesn't re-edge
  // Translate the dedicated arrow cluster + ESC into the Cardputer nav vocabulary
  // (; . , / `). Every other byte is already the ASCII the UI expects and passes
  // through untouched, so the shared App.cpp switch needs no board-specific cases.
  switch (byte) {
    case kCardKbLeft:  d.key = ','; break;
    case kCardKbUp:    d.key = ';'; break;
    case kCardKbDown:  d.key = '.'; break;
    case kCardKbRight: d.key = '/'; break;
    case kCardKbEsc:   d.key = '`'; break;
    default:           d.key = static_cast<char>(byte); break;
  }
  return d;
}

// Whether a decoded key belongs in hal::KeyState.word. Text characters do
// (printable keys AND the translated arrows/ESC, which reach the nav switch via
// word); Enter (0x0D) and Del (0x08) do NOT -- they travel as the enter/del
// flags only, matching the Cardputer whose keysState().word excludes them. This
// keeps a naive `for (char c : word)` consumer from ever seeing a stray CR/BS on
// the CoreS3 path but not the Cardputer.
inline bool is_text_key(const Decoded& d) {
  return d.key != 0 && !d.enter && !d.del;
}

}  // namespace cardkb
