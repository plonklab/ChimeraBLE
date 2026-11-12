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
#elif defined(CHIMERA_BOARD_CORES3)
  #include <M5Unified.h>
  #define BOARD M5
  static constexpr bool BOARD_CANVAS_PSRAM = true;    // CoreS3: 8 MB PSRAM
  static constexpr int  BOARD_SD_SCK  = 36;
  static constexpr int  BOARD_SD_MISO = 35;
  static constexpr int  BOARD_SD_MOSI = 37;
  static constexpr int  BOARD_SD_CS   = 4;
#else
  #error "No board selected: define CHIMERA_BOARD_CARDPUTER or CHIMERA_BOARD_CORES3 in platformio.ini"
#endif
