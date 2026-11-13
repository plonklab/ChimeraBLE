// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Pure, hardware-free input mode-machine for the LilyGo T-Embed (rotary encoder
// + encoder push + side button). Turns a gesture, in the current mode, into an
// Action; the input driver then turns that Action into a hal::KeyState or a
// character-wheel operation and keeps the Mode itself.
//
// Deliberately free of Arduino/M5 includes so it can be unit-tested on the host
// (see test/test_input/).
//
// NOTE: next_mode(Wheel, WheelCommit) returns Nav (the submit case). The driver
// overrides it to stay in Wheel when the wheel's commit was an Append; keeping
// that decision out of here is what keeps this header free of a Wheel dependency.
#pragma once

namespace tinput {

enum class Mode { Nav, Wheel };

enum class Gesture { TwistUp, TwistDown, PressShort, PressLong, BtnShort, BtnLong };

enum class Action {
  EmitUp, EmitDown, EmitSelect, EmitBack, OpenActionMenu, EnterWheel,
  WheelPrev, WheelNext, WheelCommit, EmitDel, ExitWheel, None
};

inline Action map(Mode m, Gesture g) {
  if (m == Mode::Nav) {
    switch (g) {
      case Gesture::TwistUp:    return Action::EmitUp;
      case Gesture::TwistDown:  return Action::EmitDown;
      case Gesture::PressShort: return Action::EmitSelect;
      case Gesture::PressLong:  return Action::OpenActionMenu;
      case Gesture::BtnShort:   return Action::EmitBack;
      case Gesture::BtnLong:    return Action::EnterWheel;
    }
  } else {
    switch (g) {
      case Gesture::TwistUp:    return Action::WheelPrev;
      case Gesture::TwistDown:  return Action::WheelNext;
      case Gesture::PressShort: return Action::WheelCommit;
      case Gesture::PressLong:  return Action::None;
      case Gesture::BtnShort:   return Action::EmitDel;
      case Gesture::BtnLong:    return Action::ExitWheel;
    }
  }
  return Action::None;
}

inline Mode next_mode(Mode m, Action a) {
  switch (a) {
    case Action::EnterWheel:  return Mode::Wheel;
    case Action::ExitWheel:   return Mode::Nav;
    case Action::WheelCommit: return Mode::Nav;
    default:                  return m;
  }
}

// The Cardputer nav character the shared App.cpp switch expects for a NAV emit.
inline char nav_char(Action a) {
  switch (a) {
    case Action::EmitUp:     return ';';
    case Action::EmitDown:   return '.';
    case Action::EmitSelect: return '/';
    case Action::EmitBack:   return ',';
    default:                 return 0;
  }
}

// Fixed NAV action menu: label is overlay display text, ch is the exact char
// App.cpp's switch keys on.
struct MenuItem { const char* label; char ch; };

constexpr MenuItem kActionMenu[] = {
  {"Delete",        'x'},
  {"Forget bonds",  'c'},
  {"Foxhunt audio", 'a'},
  {"Command",       '\''},
  {"Cancel",        '`'},
};
constexpr int kActionMenuCount = 5;

}  // namespace tinput
