// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Pure, hardware-free quadrature rotary-encoder decoder. Fed the raw A/B line
// levels once per input poll; returns +1 / -1 / 0 where one unit is one
// mechanical detent (a full 4-edge Gray cycle), not one edge. Invalid
// transitions (contact bounce / skipped states) contribute 0, and edges that
// reverse direction cancel, so jitter cannot double-count.
//
// Deliberately free of Arduino/M5 includes so it can be unit-tested on the
// host (see test/test_encoder/).
#pragma once

namespace enc {

class Decoder {
 public:
  // a, b: raw line levels sampled this poll. Returns +1/-1 when a detent
  // completes this sample, else 0.
  int feed(bool a, bool b) {
    // Canonical 16-entry transition table keyed on (prev<<2)|curr, where a
    // state is (A<<1)|B. Forward Gray: 00->01->11->10->00 (+1 per edge).
    static const signed char kTable[16] = {
        0, +1, -1, 0,   // prev 00
        -1, 0, 0, +1,   // prev 01
        +1, 0, 0, -1,   // prev 10
        0, -1, +1, 0};  // prev 11
    const unsigned curr = (a ? 2u : 0u) | (b ? 1u : 0u);
    acc_ += kTable[(prev_ << 2) | curr];
    prev_ = curr;
    if (acc_ >= 4)  { acc_ = 0; return +1; }
    if (acc_ <= -4) { acc_ = 0; return -1; }
    return 0;
  }

 private:
  unsigned prev_ = 0;  // last sampled (A<<1)|B; assumes detent rest at 00
  int acc_ = 0;        // net edges toward the next detent
};

}  // namespace enc
