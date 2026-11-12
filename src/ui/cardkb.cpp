// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// M5 Unit CardKB v1.1 driver (CoreS3). Reads one ASCII byte per poll from the
// keyboard at I2C 0x5F on Grove Port.A (M5Unified's external bus, M5.Ex_I2C,
// so there's no contention with the internal bus), and runs it through the
// host-tested edge/debounce decoder (cardkb_decode.h). read() maps the decode
// onto a board-neutral hal::KeyState for the shared UI.
#include "cardkb.h"

#if defined(CHIMERA_BOARD_CORES3)

#include "cardkb_decode.h"
#include "cardkb_probe.h"

namespace {
constexpr uint8_t kCardKbAddr = 0x5F;
constexpr uint32_t kCardKbFreqHz = 400000;
cardkb::Decoder g_decoder;
cardkb::Decoded g_last;   // most recent decode, surfaced by read()
cardkb::ProbeGate g_gate; // throttles the I2C probe when the CardKB is absent
}  // namespace

namespace cardkb {

void begin() {
  // External I2C on Grove Port.A (CoreS3: SDA G2 / SCL G1). M5.begin() has
  // already run, so the board's Port.A pins are known.
  M5.Ex_I2C.begin();
}

void poll() {
  uint8_t b = 0;
  const uint32_t now = millis();
  // Skip the I2C transaction entirely on throttled loops when the CardKB is
  // known-absent (b stays 0 -> decoded as "no key"), so a missing keyboard
  // doesn't pay the NAK cost every frame. g_gate re-probes every reprobe_ms to
  // catch a hot-plug.
  if (g_gate.should_probe(now)) {
    // Raw 1-byte read. If the CardKB doesn't ACK its address (absent/unplugged),
    // start() returns false and we leave b = 0 -> decoded as "no key", no hang.
    const bool acked = M5.Ex_I2C.start(kCardKbAddr, true, kCardKbFreqHz);
    if (acked) {
      M5.Ex_I2C.read(&b, 1, true);   // last_nack = true after the single byte
      M5.Ex_I2C.stop();
    }
    g_gate.update(acked, now);
  }
  g_last = g_decoder.feed(b);
}

hal::KeyState read() {
  hal::KeyState k;
  if (is_text_key(g_last)) k.word.push_back(g_last.key);   // Enter/Del are flags only (word parity with Cardputer)
  k.enter   = g_last.enter;
  k.del     = g_last.del;
  k.changed = g_last.changed;
  k.pressed = g_last.pressed;
  return k;
}

}  // namespace cardkb

#endif
