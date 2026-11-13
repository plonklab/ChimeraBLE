// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Compile-time board selection for the on-device UI. From the -DCHIMERA_BOARD_*
// flag set per env in platformio.ini, this picks the M5 global (BOARD), the
// board-specific constants, and the right M5 header. UI code uses BOARD.* and
// the BOARD_* constants and never names a specific board; the keyboard
// difference is handled separately in hal.* / cardkb.* .
#pragma once

#if defined(CHIMERA_BOARD_CARDPUTER)
  #include <M5Cardputer.h>
  #define BOARD M5Cardputer
  static constexpr bool BOARD_CANVAS_PSRAM = false;   // StampS3: no PSRAM
  static constexpr int  BOARD_SD_SCK  = 40;
  static constexpr int  BOARD_SD_MISO = 39;
  static constexpr int  BOARD_SD_MOSI = 14;
  static constexpr int  BOARD_SD_CS   = 12;
  // Paged-list visible-row counts, hand-tuned to the 240x135 panel.
  static constexpr int  BOARD_MENU_ROWS    = 4;
  static constexpr int  BOARD_DEVICE_ROWS  = 5;
  static constexpr int  BOARD_DUMP_ROWS    = 5;
  static constexpr int  BOARD_SETTING_ROWS = 5;
  static constexpr int  BOARD_SENTRY_ROWS  = 5;
#elif defined(CHIMERA_BOARD_CORES3)
  #include <M5Unified.h>
  #define BOARD M5
  static constexpr bool BOARD_CANVAS_PSRAM = true;    // CoreS3: 8 MB PSRAM
  static constexpr int  BOARD_SD_SCK  = 36;
  static constexpr int  BOARD_SD_MISO = 35;
  static constexpr int  BOARD_SD_MOSI = 37;
  static constexpr int  BOARD_SD_CS   = 4;
  // Paged-list visible-row counts for the taller 320x240 panel (fit above the
  // footer at y=222; same row heights as the Cardputer, just more rows).
  static constexpr int  BOARD_MENU_ROWS    = 9;
  static constexpr int  BOARD_DEVICE_ROWS  = 10;
  static constexpr int  BOARD_DUMP_ROWS    = 10;
  static constexpr int  BOARD_SETTING_ROWS = 9;
  static constexpr int  BOARD_SENTRY_ROWS  = 10;   // same geometry as devices/dumps (rowH 19 / y 23)
#elif defined(CHIMERA_BOARD_TEMBED)
  #include "tembed_board.h"
  #define BOARD BOARD_INSTANCE
  static constexpr bool BOARD_CANVAS_PSRAM = true;    // T-Embed: 8 MB OPI PSRAM
  static constexpr int  BOARD_SD_SCK  = 11;
  static constexpr int  BOARD_SD_MISO = 10;
  static constexpr int  BOARD_SD_MOSI = 9;
  static constexpr int  BOARD_SD_CS   = 13;
  // 320x170 landscape: shorter than CoreS3; glass-tune later.
  static constexpr int  BOARD_MENU_ROWS    = 7;
  static constexpr int  BOARD_DEVICE_ROWS  = 7;
  static constexpr int  BOARD_DUMP_ROWS    = 7;
  static constexpr int  BOARD_SETTING_ROWS = 7;
  static constexpr int  BOARD_SENTRY_ROWS  = 7;
#else
  #error "No board selected: define CHIMERA_BOARD_CARDPUTER, CHIMERA_BOARD_CORES3 or CHIMERA_BOARD_TEMBED in platformio.ini"
#endif
