// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// T-Embed CC1101 Plus input driver: rotary encoder (A=IO4, B=IO5, press=IO0)
// plus the user button (IO6) -> board-neutral hal::KeyState. Mirrors cardkb.cpp:
// poll() advances the decode once per hal::update(), read() surfaces it. The
// pure pieces (enc::Decoder, wheel::Wheel, tinput::*) are host-tested; this file
// is the hardware glue (not host-tested).
#include "tembed_input.h"

#if defined(CHIMERA_BOARD_TEMBED)

#include <Arduino.h>
#include "encoder_decode.h"
#include "wheel.h"
#include "input_map.h"

namespace {

// Pins (buttons are active-low to GND, internal pull-ups).
constexpr int kPinEncA   = 4;
constexpr int kPinEncB   = 5;
constexpr int kPinEncBtn = 0;   // also the BOOT strap; fine as a runtime button
constexpr int kPinUser   = 6;

constexpr uint32_t kDebounceMs  = 20;   // level must be stable this long
constexpr uint32_t kLongPressMs = 500;  // builder-tunable long-press threshold

const char* const kRibbon =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_.,:/@#!?+=()&%*";

enum class Press { None, Short, Long };

// Debounced button. Long is reported ONCE at the threshold crossing while still
// held (and the following release is swallowed); Short is reported on release
// only if Long never fired. So a press yields exactly one classification.
struct Button {
  int      pin = 0;
  bool     raw = false;          // last raw sample (true = pressed)
  bool     stable = false;       // debounced state
  bool     longFired = false;
  uint32_t rawSince = 0;         // when raw last changed
  uint32_t downAt = 0;           // when debounced press began

  void init(int p) { pin = p; raw = stable = longFired = false; rawSince = downAt = millis(); }

  Press step(uint32_t now) {
    const bool lvl = (digitalRead(pin) == LOW);
    if (lvl != raw) { raw = lvl; rawSince = now; }
    Press out = Press::None;
    if (raw != stable && (now - rawSince) >= kDebounceMs) {
      stable = raw;
      if (stable) { downAt = now; longFired = false; }
      else if (!longFired) out = Press::Short;
    }
    if (stable && !longFired && (now - downAt) >= kLongPressMs) {
      longFired = true;
      out = Press::Long;
    }
    return out;
  }
};

enc::Decoder   g_dec;
wheel::Wheel   g_wheel(kRibbon);
tinput::Mode   g_mode = tinput::Mode::Nav;
hal::KeyState  g_pending;
Button         g_encBtn, g_userBtn;

// Sample all inputs once; reduce to at most one gesture. Returns false if none.
bool sample(tinput::Gesture& g) {
  const uint32_t now = millis();
  const int step = g_dec.feed(digitalRead(kPinEncA) == HIGH, digitalRead(kPinEncB) == HIGH);
  const Press pe = g_encBtn.step(now);
  const Press pu = g_userBtn.step(now);
  // Priority: presses over twists (a press is the rarer, more deliberate act).
  if (pe == Press::Long)  { g = tinput::Gesture::PressLong;  return true; }
  if (pe == Press::Short) { g = tinput::Gesture::PressShort; return true; }
  if (pu == Press::Long)  { g = tinput::Gesture::BtnLong;    return true; }
  if (pu == Press::Short) { g = tinput::Gesture::BtnShort;   return true; }
  if (step > 0) { g = tinput::Gesture::TwistUp;   return true; }
  if (step < 0) { g = tinput::Gesture::TwistDown; return true; }
  return false;
}

void push_event(hal::KeyState& k) { k.changed = true; k.pressed = true; }

// Blocking action-menu modal. Returns the chosen char (0 if dismissed).
char run_action_menu() {
  auto& d = BOARD.Display;
  int sel = 0;
  bool dirty = true;
  const int n = tinput::kActionMenuCount;
  const int rowH = 20, w = 200;
  const int h = n * rowH + 8;
  const int x = (d.width() - w) / 2, y = (d.height() - h) / 2;
  for (;;) {
    if (dirty) {
      d.fillRect(x, y, w, h, TFT_BLACK);
      d.drawRect(x, y, w, h, TFT_WHITE);
      d.setTextSize(2);
      d.setTextDatum(textdatum_t::top_left);
      for (int i = 0; i < n; ++i) {
        const int ry = y + 4 + i * rowH;
        if (i == sel) d.fillRect(x + 2, ry, w - 4, rowH, TFT_WHITE);
        d.setTextColor(i == sel ? TFT_BLACK : TFT_WHITE, i == sel ? TFT_WHITE : TFT_BLACK);
        d.drawString(tinput::kActionMenu[i].label, x + 8, ry + 2);
      }
      dirty = false;
    }
    tinput::Gesture g;
    if (sample(g)) {
      switch (g) {
        case tinput::Gesture::TwistUp:    sel = (sel + n - 1) % n; dirty = true; break;
        case tinput::Gesture::TwistDown:  sel = (sel + 1) % n;     dirty = true; break;
        case tinput::Gesture::PressShort: return tinput::kActionMenu[sel].ch;
        case tinput::Gesture::BtnShort:
        case tinput::Gesture::BtnLong:    return 0;
        default: break;   // PressLong (the opening hold's tail) ignored
      }
    }
    delay(2);
  }
}

}  // namespace

namespace tembed_input {

void begin() {
  pinMode(kPinEncA,   INPUT_PULLUP);
  pinMode(kPinEncB,   INPUT_PULLUP);
  pinMode(kPinEncBtn, INPUT_PULLUP);
  pinMode(kPinUser,   INPUT_PULLUP);
  g_dec   = enc::Decoder();
  g_wheel = wheel::Wheel(kRibbon);
  g_mode  = tinput::Mode::Nav;
  g_pending = hal::KeyState{};
  g_encBtn.init(kPinEncBtn);
  g_userBtn.init(kPinUser);
}

void poll() {
  g_pending = hal::KeyState{};   // fresh each poll, like cardkb's g_last
  tinput::Gesture g;
  if (!sample(g)) return;

  const tinput::Action a = tinput::map(g_mode, g);
  switch (a) {
    case tinput::Action::EmitUp:
    case tinput::Action::EmitDown:
    case tinput::Action::EmitSelect:
    case tinput::Action::EmitBack:
      g_pending.word.push_back(tinput::nav_char(a));
      push_event(g_pending);
      break;
    case tinput::Action::WheelPrev: g_wheel.prev(); break;   // UI draws it; no key event
    case tinput::Action::WheelNext: g_wheel.next(); break;
    case tinput::Action::WheelCommit: {
      const wheel::Commit c = g_wheel.commit();
      if (c.kind == wheel::CommitKind::Append) {
        g_pending.word.push_back(c.ch);   // stay in Wheel mode (overrides next_mode)
      } else {
        g_pending.enter = true;
        g_mode = tinput::Mode::Nav;
      }
      push_event(g_pending);
      break;
    }
    case tinput::Action::EmitDel:
      g_pending.del = true;
      push_event(g_pending);
      break;
    case tinput::Action::EnterWheel:
    case tinput::Action::ExitWheel:
      g_mode = tinput::next_mode(g_mode, a);
      break;
    case tinput::Action::OpenActionMenu: {
      const char ch = run_action_menu();
      if (ch) { g_pending.word.push_back(ch); push_event(g_pending); }
      break;
    }
    case tinput::Action::None: break;
  }
}

hal::KeyState read() {
  hal::KeyState k = g_pending;
  g_pending = hal::KeyState{};
  return k;
}

}  // namespace tembed_input

#endif  // CHIMERA_BOARD_TEMBED
