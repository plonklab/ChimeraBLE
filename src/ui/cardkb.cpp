// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// M5 Unit CardKB v1.1 driver (CoreS3). Skeleton: begin/poll/read exist and
// compile so the cores3 target boots with the shared UI; the real Port.A I2C
// read (addr 0x5F) and the edge/debounce state machine are added in the next
// task (with a host-tested pure decoder). Until then read() reports no key, so
// first-boot shows the splash + main menu without keyboard nav.
#include "cardkb.h"

#if defined(CHIMERA_BOARD_CORES3)

namespace cardkb {

void begin() {
  // Task 3: bring up the Port.A I2C bus (SDA G2 / SCL G1) for CardKB @0x5F.
}

void poll() {
  // Task 3: read one byte from 0x5F, feed the edge/debounce state machine.
}

hal::KeyState read() {
  return hal::KeyState{};   // Task 3: real key edges
}

}  // namespace cardkb

#endif
