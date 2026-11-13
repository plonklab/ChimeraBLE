// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// T-Embed CC1101 Plus input (rotary encoder + keys). Declaration only; the
// implementation lands with the input task. Mirrors cardkb.h.
#pragma once

#include "hal.h"

namespace tembed_input {
void         begin();
void         poll();
hal::KeyState read();
}  // namespace tembed_input
