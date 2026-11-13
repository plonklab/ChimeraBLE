// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Pure, hardware-free character-wheel (text-entry ribbon) model for the
// T-Embed rotary encoder. The ribbon is the selectable characters; an "OK"
// sentinel is appended as the final slot, so the row directly above the
// first character (home, index 0) is OK. next()/prev() wrap. commit() yields
// Append(ch) for a character or Submit at OK.
//
// Free of Arduino includes so it can be host-tested (see test/test_wheel/).
#pragma once

#include <cstring>
#include <string>

namespace wheel {

enum class CommitKind { Append, Submit };

struct Commit {
  CommitKind kind;
  char ch;  // valid only when kind == Append
};

class Wheel {
 public:
  explicit Wheel(const char* ribbon) : ribbon_(ribbon ? ribbon : "") {}

  void next() { idx_ = wrap(idx_ + 1); }
  void prev() { idx_ = wrap(idx_ - 1); }

  bool at_ok() const { return idx_ == size() - 1; }
  char current() const { return at_ok() ? '\0' : ribbon_[idx_]; }

  Commit commit() const {
    if (at_ok()) return Commit{CommitKind::Submit, '\0'};
    return Commit{CommitKind::Append, ribbon_[idx_]};
  }

  std::string label_at_offset(int off) const {
    int i = wrap(idx_ + off);
    if (i == size() - 1) return "OK";
    return std::string(1, ribbon_[i]);
  }

  int index() const { return idx_; }
  int size() const { return static_cast<int>(ribbon_.size()) + 1; }

 private:
  int wrap(int i) const {
    int n = size();
    return ((i % n) + n) % n;
  }

  std::string ribbon_;
  int idx_ = 0;
};

}  // namespace wheel
