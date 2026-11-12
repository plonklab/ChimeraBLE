// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Host unit tests for the pure CardKB edge/debounce decoder (cardkb_decode.h).
// Run: pio test -e native_test -f test_cardkb
#include <unity.h>

#include "cardkb_decode.h"

using cardkb::Decoded;
using cardkb::Decoder;

// A fresh key raises exactly one down-edge; holding it (same byte) does not
// re-raise; releasing (0) and re-pressing raises again. This is the guard
// against a held key over-advancing a menu (Review Focus).
void test_fresh_byte_raises_one_edge() {
  Decoder d;

  Decoded a = d.feed('a');
  TEST_ASSERT_TRUE(a.changed);
  TEST_ASSERT_TRUE(a.pressed);
  TEST_ASSERT_EQUAL_CHAR('a', a.key);

  Decoded held = d.feed('a');          // same byte still down -> no new edge
  TEST_ASSERT_FALSE(held.changed);
  TEST_ASSERT_TRUE(held.pressed);

  d.feed(0);                            // release
  Decoded again = d.feed('a');         // re-press -> edge again
  TEST_ASSERT_TRUE(again.changed);
}

// Enter (0x0D) and Backspace/Del (0x08) set their flags; a normal key does not.
void test_enter_and_del_flags() {
  Decoder d;

  Decoded cr = d.feed(0x0D);
  TEST_ASSERT_TRUE(cr.enter);
  TEST_ASSERT_FALSE(cr.del);

  d.feed(0);
  Decoded bs = d.feed(0x08);
  TEST_ASSERT_TRUE(bs.del);
  TEST_ASSERT_FALSE(bs.enter);

  d.feed(0);
  Decoded x = d.feed('x');
  TEST_ASSERT_FALSE(x.enter);
  TEST_ASSERT_FALSE(x.del);
}

// No byte available (0) = no key, no edge, no throw. Covers "CardKB absent /
// nothing pressed" (Review Focus).
void test_no_key_is_quiet() {
  Decoder d;
  Decoded n = d.feed(0);
  TEST_ASSERT_FALSE(n.changed);
  TEST_ASSERT_FALSE(n.pressed);
  TEST_ASSERT_EQUAL_CHAR(0, n.key);
}

// The CardKB's dedicated arrow cluster + ESC are non-ASCII high bytes the UI
// doesn't understand. The decoder translates them into the Cardputer nav
// vocabulary (; . , / `) so the shared App.cpp switch needs no board-specific
// cases and the Cardputer build stays byte-identical. Left->back, Right->select
// mirrors the Cardputer; ESC maps to the top-left '`' action-menu key.
void test_arrows_and_esc_map_to_nav_chars() {
  Decoder d;

  Decoded up = d.feed(cardkb::kCardKbUp);
  TEST_ASSERT_TRUE(up.changed);
  TEST_ASSERT_TRUE(up.pressed);
  TEST_ASSERT_EQUAL_CHAR(';', up.key);

  d.feed(0);
  Decoded down = d.feed(cardkb::kCardKbDown);
  TEST_ASSERT_EQUAL_CHAR('.', down.key);

  d.feed(0);
  Decoded left = d.feed(cardkb::kCardKbLeft);
  TEST_ASSERT_EQUAL_CHAR(',', left.key);

  d.feed(0);
  Decoded right = d.feed(cardkb::kCardKbRight);
  TEST_ASSERT_EQUAL_CHAR('/', right.key);

  d.feed(0);
  Decoded esc = d.feed(cardkb::kCardKbEsc);
  TEST_ASSERT_EQUAL_CHAR('`', esc.key);

  // A translated arrow carries neither the enter nor the del flag.
  TEST_ASSERT_FALSE(right.enter);
  TEST_ASSERT_FALSE(right.del);
}

// Holding a translated arrow (same raw byte repeated) must NOT re-raise an edge,
// exactly like a held ASCII key -- otherwise one held press runs the menu away.
void test_held_arrow_raises_one_edge() {
  Decoder d;

  Decoded first = d.feed(cardkb::kCardKbDown);
  TEST_ASSERT_TRUE(first.changed);
  TEST_ASSERT_EQUAL_CHAR('.', first.key);

  Decoded held = d.feed(cardkb::kCardKbDown);   // still held -> no new edge
  TEST_ASSERT_FALSE(held.changed);
  TEST_ASSERT_TRUE(held.pressed);
  TEST_ASSERT_EQUAL_CHAR('.', held.key);

  d.feed(0);                                     // release
  Decoded again = d.feed(cardkb::kCardKbDown);   // re-press -> edge again
  TEST_ASSERT_TRUE(again.changed);
}

// KeyState.word must hold text characters only -- Enter (0x0D) and Del (0x08)
// travel as the enter/del flags, never in word, matching the Cardputer (whose
// keysState().word excludes them). Translated arrows/ESC ARE text (they reach
// the nav switch via word), so they stay in. Guards the CoreS3-only "stray
// CR/BS in word" parity trap.
void test_word_holds_text_keys_only() {
  Decoder d;

  TEST_ASSERT_TRUE(cardkb::is_text_key(d.feed('a')));          // printable -> in word
  d.feed(0);
  TEST_ASSERT_FALSE(cardkb::is_text_key(d.feed(0x0D)));        // Enter -> flag only
  d.feed(0);
  TEST_ASSERT_FALSE(cardkb::is_text_key(d.feed(0x08)));        // Del   -> flag only
  d.feed(0);
  TEST_ASSERT_TRUE(cardkb::is_text_key(d.feed(cardkb::kCardKbRight)));  // arrow -> text
  d.feed(0);
  TEST_ASSERT_TRUE(cardkb::is_text_key(d.feed(cardkb::kCardKbEsc)));    // ESC   -> text
  d.feed(0);
  TEST_ASSERT_FALSE(cardkb::is_text_key(d.feed(0)));           // no key -> not text
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_byte_raises_one_edge);
  RUN_TEST(test_enter_and_del_flags);
  RUN_TEST(test_no_key_is_quiet);
  RUN_TEST(test_arrows_and_esc_map_to_nav_chars);
  RUN_TEST(test_held_arrow_raises_one_edge);
  RUN_TEST(test_word_holds_text_keys_only);
  return UNITY_END();
}
