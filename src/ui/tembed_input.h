// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// T-Embed CC1101 Plus input (rotary encoder + keys). Declaration only; the
// implementation lands with the input task. Mirrors cardkb.h.
#pragma once

#include <string>
#include "hal.h"

namespace tembed_input {
void         begin();
void         poll();
hal::KeyState read();

// Read-only wheel view for the UI to render (see UI.cpp drawWheelOverlay).
bool         wheel_active();                 // true while in Wheel mode
std::string  wheel_label(int offset);        // offset -2..+2; "OK" or one char
}  // namespace tembed_input
