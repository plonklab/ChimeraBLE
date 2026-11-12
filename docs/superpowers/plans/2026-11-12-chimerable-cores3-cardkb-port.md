# ChimeraBLE CoreS3 + CardKB Port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `cores3` on-device-UI target (M5Stack CoreS3 + CardKB keyboard) to ChimeraBLE, sharing one `src/ui/` codebase with the existing Cardputer target via a compile-time board HAL, without changing the `esp32dev`/`xiao_esp32c5`/`cardputer` builds.

**Architecture:** The `src/ui/` front-end stops calling the `M5Cardputer` global directly and goes through a compile-time board shim (`board.h` + a small `hal::` keyboard contract). `M5Cardputer` *is* an `M5Unified` subclass, so Display/Speaker/Power/update are a rename; `begin()` is a per-board shim; the keyboard is the one real divergence (Cardputer matrix KB → CardKB I2C @0x5F on Port.A). The BLE engine, CLI, and serial targets are untouched.

**Tech Stack:** pioarduino (Arduino-ESP32 3.x / ESP-IDF 5.5), NimBLE-Arduino 2.x, M5Unified/M5GFX (cores3) / M5Cardputer (cardputer), ArduinoJson. Native Unity host test for the CardKB state machine.

**Spec:** `docs/superpowers/specs/2026-11-12-chimerable-cores3-cardkb-port-design.md`

## Global Constraints

- Platform stays the shared pioarduino `55.03.34`; do **not** bump it (would perturb the other targets). If M5Unified 0.2.29 needs newer, fall back to PlonkWave's proven `55.03.312` — and only then.
- `cores3` libs pinned: `https://github.com/m5stack/M5GFX.git#0.2.36` **then** `...M5Unified.git#0.2.29` (M5GFX first so its dep is satisfied). NOT M5Cardputer.
- **No engine edits.** Only `src/ui/*`, `platformio.ini`, `tools/*.sh`, and new `src/ui/board.h` / `src/ui/cardkb.*` change. If an engine file (`src/*.cpp|h` outside `ui/`) needs changing, stop.
- GPL-3.0-or-later `SPDX` + `Copyright (C) 2026 Matthew Kukanich` header on every file, including new ones.
- The `esp32dev`, `xiao_esp32c5`, and `cardputer` builds must stay green after every task.
- CardKB: I2C `@0x5F` on Port.A (`SDA G2 / SCL G1`), dedicated `Wire`; resolves modifiers on-device → plain ASCII. CoreS3 SD pins: `SCK 36 / MISO 35 / MOSI 37 / CS 4`. CoreS3 canvas → PSRAM.
- **SD is the storage contract** (launcher deployment = no runtime LittleFS). Ship **both** artifacts: `ChimeraBLE_cores3_merged.bin` (direct 0x0) + `ChimeraBLE_cores3_M5Launcher.bin` (app-only).

## Review Focus

- **CardKB held key / auto-repeat → menu over-advances.** A held or repeating byte must produce one down-edge, not continuous motion. → Task 3 host test (`held byte = one edge`).
- **No SD card at boot → must still boot + run** (no OUI = no vendor names, but no crash/hang). → Task 5 verify (mirror `App.cpp:231` graceful handling).
- **CardKB absent/unplugged → no I2C hang at boot.** A missing 0x5F must not block `hal::update()`. → Task 3 (non-blocking read, host test `no device = no key, no throw`).
- **Launcher clone-advertise reboot returns to ChimeraBLE, not the launcher.** → Task 7 HIL.
- **HAL refactor changes Cardputer/serial behavior.** → Task 1 gate: all three existing envs build green and the HAL for Cardputer is pure pass-through (reviewable line-by-line).

---

### Task 1: Board HAL extraction (Cardputer pass-through) — prove the others unbroken

**Files:**
- Create: `src/ui/board.h` (board selection + constants), `src/ui/hal.h` (`hal::KeyState`, `hal::` keyboard/init/update API)
- Modify: `src/ui/UI.cpp`, `src/ui/UI.h`, `src/ui/App.cpp` (route every `M5Cardputer.*` site through `BOARD`/`hal::`), `platformio.ini` (`cardputer` env gains `-DCHIMERA_BOARD_CARDPUTER`)
- (branch setup folded into Step 1)

**Interfaces — Produces** (what Tasks 2–7 rely on; exact names/types):
- `BOARD` — macro → the M5Unified-family global (`M5Cardputer` here). `BOARD.Display`, `BOARD.Speaker`, `BOARD.Power` usable as today.
- `hal::KeyState { std::vector<char> word; bool enter; bool del; bool changed; bool pressed; }`
- `hal::begin(const m5::M5Unified::config_t& cfg)` — board init (Cardputer: `M5Cardputer.begin(cfg,true)`).
- `hal::update()` — per-loop pump (Cardputer: `M5Cardputer.update()`).
- `hal::KeyState hal::readKeyboard()` — current key edge (Cardputer: wraps `M5Cardputer.Keyboard`).
- Constants: `BOARD_CANVAS_PSRAM` (bool), `BOARD_SD_SCK/MISO/MOSI/CS` (int). Cardputer: `false`, `40/39/14/12`.

- [ ] **Step 1: Branch setup.** `git switch -c develop` (from `main`), `git switch -c feat/cores3-cardkb`; `git add` the design doc + this plan; commit `docs: cores3+cardkb port design + plan`.
- [ ] **Step 2: Baseline-build all three existing envs** `pio run -e esp32dev -e xiao_esp32c5 -e cardputer`. Expected: all SUCCESS. Record flash/RAM of `cardputer` (the before number).
- [ ] **Step 3: Write `src/ui/hal.h` + `src/ui/board.h`** — the Produces interfaces above; the Cardputer branch (`#ifdef CHIMERA_BOARD_CARDPUTER`) is a pure pass-through to `M5Cardputer`. GPL header on both.
- [ ] **Step 4: Route `src/ui/` through the HAL.** Replace the `M5Cardputer.*` sites (per the design doc: 7 `update()`, ~8 `Keyboard.*` across 7 functions, UI.cpp `begin`/`Display`/`pushSprite`, Speaker/Power) with `hal::`/`BOARD.`. `handleKeyboard()` + the 6 blocking helpers read `hal::readKeyboard()` (use `.changed`/`.pressed` where they used `isChange()/isPressed()`). `UI.cpp:53` PSRAM uses `BOARD_CANVAS_PSRAM`; `App.cpp:89` SD pins use `BOARD_SD_*`.
- [ ] **Step 5: Add `-DCHIMERA_BOARD_CARDPUTER`** to the `cardputer` env in `platformio.ini`.
- [ ] **Step 6: Rebuild + gate.** `pio run -e esp32dev -e xiao_esp32c5 -e cardputer`. Expected: all SUCCESS; `cardputer` flash/RAM within a few hundred bytes of Step 2 (a pure pass-through compiles ~identically). Read `${PIPESTATUS}`/the per-env SUCCESS banner, not a piped tail.
- [ ] **Step 7: Commit** `refactor(ui): extract compile-time board HAL (cardputer pass-through)`.

---

### Task 2: `cores3` env + board.h CoreS3 branch + first boot

**Files:**
- Modify: `platformio.ini` (new `[env:cores3]`), `src/ui/board.h` (`#ifdef CHIMERA_BOARD_CORES3` branch)
- Create: `src/ui/cardkb.h`, `src/ui/cardkb.cpp` (skeleton: `Wire` on G2/G1 + a stub `readKeyboard()` returning an empty `KeyState` for now)

**Interfaces — Consumes:** Task 1's `hal::`/`BOARD` API. **Produces:** a compiling, bootable `cores3` firmware; `hal::begin` on CoreS3 = `M5.begin(cfg)` + `cardkb::begin()` (Wire on G2/G1 after `M5.begin`).

- [ ] **Step 1: Add `[env:cores3]`** per the spec (board `m5stack-cores3`, 16MB flash + `default_16MB.csv`, `build_src_filter = +<*> -<main.cpp>`, `-DCHIMERA_BOARD_CORES3 -DARDUINO_USB_CDC_ON_BOOT=1`, M5GFX#0.2.36 + M5Unified#0.2.29).
- [ ] **Step 2: board.h CoreS3 branch** — `BOARD`=`M5`, `hal::begin`=`M5.begin(cfg)`+`cardkb::begin()`, `hal::update`=`M5.update()`+`cardkb::poll()`, constants `BOARD_CANVAS_PSRAM=true`, SD `36/35/37/4`. `cardkb.{h,cpp}` skeleton (Wire init, stub poll/readKeyboard).
- [ ] **Step 3: Build** `pio run -e cores3`. Expected: SUCCESS (if the platform rejects M5Unified 0.2.29, switch this env's `platform` to `55.03.312` per Global Constraints, rebuild).
- [ ] **Step 4: Re-gate the others** `pio run -e cardputer -e esp32dev -e xiao_esp32c5` still SUCCESS.
- [ ] **Step 5: HIL (Silabel flashes, Aaron confirms).** Flash `pio run -e cores3 -t upload` (kill any monitor first). Expected on glass: boot splash + main menu render (nav not wired yet). Aaron reports.
- [ ] **Step 6: Commit** `feat(cores3): add env + board HAL branch + first boot`.

---

### Task 3: CardKB driver + edge/debounce state machine (host-tested)

**Files:**
- Modify: `src/ui/cardkb.cpp`, `src/ui/cardkb.h`
- Create: `src/ui/cardkb_decode.h` (pure, hardware-free: the state machine as a testable function), `test/test_cardkb/test_cardkb_decode.cpp`, `[env:native_test]` in `platformio.ini`

**Interfaces — Consumes:** Task 2 skeleton. **Produces:** `hal::readKeyboard()` on CoreS3 yields correct edges from CardKB bytes.

Pure decode under test (no Arduino/Wire):
`cardkb::Decoded cardkb::feed(uint8_t byte)` where `byte==0` means "no key this poll"; a fresh non-zero byte raises `changed=true, pressed=true` for exactly one call and fills `word=[byte], enter=(byte==0x0D), del=(byte==0x08)`; a repeat of the same byte within the hold window does **not** re-raise `changed` (debounce); `0` resets so the next non-zero re-raises.

- [ ] **Step 1: Write failing host tests** `test/test_cardkb/test_cardkb_decode.cpp`:
  - `fresh_byte_raises_one_edge`: feed `'a'` → changed&pressed, word==`['a']`; feed `'a'` again → !changed; feed `0` then `'a'` → changed again.
  - `enter_and_del_flags`: feed `0x0D` → enter; feed `0x08` → del.
  - `no_key_is_quiet`: feed `0` → !changed && !pressed && word empty (covers "CardKB absent → no key, no throw").
- [ ] **Step 2: Run, verify FAIL** `pio test -e native_test -f test_cardkb`. Expected: FAIL (no `feed`).
- [ ] **Step 3: Implement `cardkb::feed`** in `cardkb_decode.h` (pure last-byte edge machine). Wire `cardkb.cpp` to read 1 byte from 0x5F on the Port.A `Wire` each `poll()`, non-blocking (short timeout → treat as `0`), feed it through `feed()`, expose the result via `hal::readKeyboard()`.
- [ ] **Step 4: Run, verify PASS** `pio test -e native_test -f test_cardkb`. Expected: PASS.
- [ ] **Step 5: Build cores3 + re-gate others** `pio run -e cores3 -e cardputer -e esp32dev -e xiao_esp32c5`. All SUCCESS.
- [ ] **Step 6: HIL.** Flash cores3; CardKB nav works on glass — `;`/`.` move, `/`/Enter open, `,`/Del back, `'` command-mode + type a command. Confirm no multi-advance on a held key (the open bring-up fact: tune the hold window if it streams). Aaron reports.
- [ ] **Step 7: Commit** `feat(cores3): CardKB I2C keyboard + edge state machine (host-tested)`.

---

### Task 4: Layout re-flow for 320×240

**Files:** Modify `src/ui/UI.cpp` (per-screen metrics). No engine/HAL change.

- [ ] **Step 1: Build+flash cores3**, walk every screen (Main/Recon/Adversarial/Devices/Detail/GATT/HidStream/Foxhunt/Clone/Dumps/DumpView/Settings/Honeypot/Sentry*/Alert/About/Logs); note clipping/under-fill/tiny-text per screen.
- [ ] **Step 2: Tune screen-by-screen** — row counts for the taller panel, font sizes, foxhunt bar width, header/footer proportions. Canvas auto-sizes from `BOARD.Display` w/h (already dynamic), so this is proportion tuning, not a rewrite.
- [ ] **Step 3: Re-gate** `pio run -e cardputer` still SUCCESS (shared `UI.cpp` — confirm Cardputer layout still sane; if a constant was Cardputer-tuned, keep it behind a `BOARD_*` metric rather than hardcoding CoreS3 values).
- [ ] **Step 4: HIL** sweep — Aaron confirms each screen reads well at 320×240.
- [ ] **Step 5: Commit** `feat(cores3): re-flow UI layout for 320x240`.

---

### Task 5: SD bring-up (CoreS3 pins) + OUI / dumps / clones / honeypot

**Files:** Modify `src/ui/App.cpp` (SD init uses `BOARD_SD_*` from Task 1; no new storage logic — the SD-first paths already exist).

- [ ] **Step 1: Confirm `sdInit()` uses `BOARD_SD_*`** (not the Cardputer literals) — folded in Task 1 Step 4; verify here.
- [ ] **Step 2: Build cores3 + re-gate others.** All SUCCESS.
- [ ] **Step 3: HIL — card present.** SD with `/ChimeraBLE/oui.bin`: boot → vendor names resolve in `list`/`info`; `dump` auto-saves to `/ChimeraBLE/ble_dumps/`; clone save + honeypot log write. Aaron reports.
- [ ] **Step 4: HIL — no card** (Review Focus): boot with no SD → app runs, scan/connect/dump work, vendor names just absent, **no crash/hang**.
- [ ] **Step 5: Commit** `feat(cores3): SD storage on CoreS3 pins (OUI/dumps/clones/honeypot)`.

---

### Task 6: Launcher + direct build tooling (both artifacts)

**Files:** Modify `tools/make_merged_bin.sh` (add `cores3` case: `CHIP=esp32s3`, `BOOT_OFF=0x0`, `FS_OFF` per `default_16MB.csv`), `tools/build_release.sh` (add `cores3` to the merged loop + emit `ChimeraBLE_cores3_M5Launcher.bin` from `.pio/build/cores3/firmware.bin`). Update `README.md` supported-boards table + CoreS3 flashing/SD notes.

- [ ] **Step 1: `make_merged_bin.sh` cores3 case**; run `tools/make_merged_bin.sh cores3`. Expected: writes `cores3_merged.bin`, prints a size.
- [ ] **Step 2: `build_release.sh` cores3**; run it. Expected: `dist/ChimeraBLE_cores3_merged.bin` + `dist/ChimeraBLE_cores3_M5Launcher.bin` + `oui.bin`.
- [ ] **Step 3: README** — add CoreS3 row + "both builds" + SD `/ChimeraBLE/oui.bin` + launcher-load note. Keep Matthew's credits/license intact.
- [ ] **Step 4: HIL — launcher load.** Copy `ChimeraBLE_cores3_M5Launcher.bin` to SD, load via bmorcelli's launcher. Expected: boots into ChimeraBLE, CardKB + SD both work (this is the first all-up launcher run). Aaron reports.
- [ ] **Step 5: Commit** `feat(cores3): merged + launcher release artifacts + README`.

---

### Task 7: Full feature sweep via the launcher (acceptance)

**Files:** none expected (bug-fix commits only if HIL finds something).

- [ ] **Step 1: HIL acceptance, launcher-loaded.** scan → list (vendor names) → info → connect → secure → dump → hid/hid stream → clone save → **clone advertise** (verify the MAC-spoof reboot returns to ChimeraBLE, not the launcher — Review Focus) → foxhunt → sentry add/watch/alert → mitm. Aaron drives/observes.
- [ ] **Step 2: Fix anything HIL surfaces** (own commits, re-gate all envs build-green each time).
- [ ] **Step 3: Open PR** `feat/cores3-cardkb` → `develop` once the sweep is clean — the finished, reviewable unit. **Aaron merges** (Silabel never).
