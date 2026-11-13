// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Matthew Kukanich
//
// Pure, hardware-free precedence policy for the OUI vendor database source on
// the on-device-UI boards (Cardputer / CoreS3), shared through src/ui/App.cpp.
//
// The merged (0x0) firmware images bake an oui.bin into a LittleFS partition,
// but the app prefers the microSD card: an SD oui.bin is removable and can be
// refreshed without reflashing, while the embedded copy is frozen at release.
// So SD wins whenever it already supplied a usable DB; the embedded copy is a
// fallback for when there's no SD, or the SD has no oui.bin on it.
//
// The embedded DB is only reachable on a *direct* 0x0 flash. Under the M5 /
// bmorcelli launcher a different partition table is active, so our LittleFS
// partition isn't present -- hence the lfs_mounted / lfs_oui_present guards,
// which keep us from trying to read a filesystem that isn't there.
//
// Logic is injected (three booleans) so the whole policy is unit-testable on
// the host with no Arduino/M5/FS dependency (see test/test_oui/).
#pragma once

namespace oui_src {

// Decide whether to fall back to the firmware-embedded LittleFS OUI database.
//   already_loaded   -- a usable DB was already opened (from SD). SD wins: if
//                       true we must NOT override it with the frozen embedded copy.
//   lfs_mounted      -- the LittleFS partition mounted this boot.
//   lfs_oui_present  -- /oui.bin actually exists on that LittleFS.
// Returns true only when nothing is loaded yet AND a mounted LittleFS really
// carries the embedded DB.
inline bool use_embedded(bool already_loaded, bool lfs_mounted, bool lfs_oui_present) {
  return !already_loaded && lfs_mounted && lfs_oui_present;
}

}  // namespace oui_src
