// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// M5 Unit CardKB v1.1 driver (CoreS3 only). The CardKB is an I2C keyboard
// (addr 0x5F) on Grove Port.A; it resolves Shift/Sym/Fn on-device and returns
// plain ASCII. poll() is called once per hal::update(); read() returns the
// current key edge as a board-neutral hal::KeyState. The edge/debounce state
// machine and the actual I2C read land in a later task; this header is the
// contract hal.cpp (CoreS3 branch) consumes.
#pragma once

#include "hal.h"   // hal::KeyState

namespace cardkb {

void          begin();   // bring up the Port.A I2C bus for the CardKB
void          poll();    // read one key byte + advance the edge machine (per hal::update())
hal::KeyState read();     // the current key edge/state, as a board-neutral KeyState

}  // namespace cardkb
