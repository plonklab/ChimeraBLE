// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Pure, hardware-free probe-cadence gate for the CardKB poll (CoreS3).
//
// cardkb::poll() runs every hal::update() (~100x/sec). When the CardKB is
// present the I2C address probe ACKs instantly, so probing every loop is cheap.
// When it is ABSENT, each probe pays the I2C NAK/timeout cost for nothing, every
// frame. This gate caches presence: probe every loop while present, but once a
// probe NAKs, throttle to one re-probe per reprobe_ms so a missing keyboard
// stops taxing the loop -- while still recovering on hot-plug within reprobe_ms.
//
// Clock is injected (now_ms) so the whole policy is unit-testable on the host
// with no Arduino/M5 dependency (see test/test_cardkb/). cardkb.cpp owns the
// millis() clock and the actual I2C transaction.
#pragma once

#include <cstdint>

namespace cardkb {

class ProbeGate {
 public:
  explicit ProbeGate(uint32_t reprobe_ms = 1000) : reprobe_ms_(reprobe_ms) {}

  // Call once per poll with the current clock. true = do the full I2C probe this
  // loop. Present -> always; absent -> only once reprobe_ms has elapsed since the
  // last probe. Starts optimistic (present_) so the very first loop probes.
  bool should_probe(uint32_t now_ms) const {
    return present_ || (now_ms - last_probe_ms_) >= reprobe_ms_;
  }

  // Feed back the probe outcome: acked = the CardKB answered. Updates presence
  // and the throttle mark. (Only called on loops where should_probe() was true.)
  void update(bool acked, uint32_t now_ms) {
    present_       = acked;
    last_probe_ms_ = now_ms;
  }

  bool present() const { return present_; }

 private:
  uint32_t reprobe_ms_;
  bool     present_       = true;   // optimistic at boot -> first loop probes
  uint32_t last_probe_ms_ = 0;
};

}  // namespace cardkb
