# ChimeraBLE — M5Stack CoreS3 + CardKB front-end port

**Design doc** · 2026-11-12 · fork `plonklab/ChimeraBLE` (of `MatthewKuKanich/ChimeraBLE`, GPL-3.0)

## Intent / why

Run ChimeraBLE on an **M5Stack CoreS3** with an **M5 Unit CardKB v1.1** as its keyboard —
hardware Aaron owns — as a third on-device-UI target, **without disturbing the existing
`esp32dev`, `xiao_esp32c5`, or `cardputer` builds**. This is a learning-driven port of an
abandoned-but-stable upstream tool; the point is a clean hardware-abstraction seam Aaron can
reuse, not just getting pixels on a screen.

It is a **re-skin + input swap, not a rewrite**: CoreS3 and the existing Cardputer target are
both ESP32-S3, share the BLE radio, the NimBLE stack, the pioarduino framework, and the entire
ChimeraBLE engine. What changes is confined to the on-device front-end in `src/ui/`.

## Goals

1. A working `cores3` build env that runs the full ChimeraBLE engine + the existing on-device
   menu UI, driven by the CardKB keyboard, rendered on the 320×240 display.
2. Extract a **compile-time board HAL** so the Cardputer and CoreS3 front-ends share one UI
   codebase; board differences live behind a thin per-board shim.
3. The Cardputer target compiles and behaves **identically** after the refactor (the HAL for it
   is a pass-through to `M5Cardputer`).
4. The serial targets (`esp32dev`, `xiao_esp32c5`) are untouched — they don't compile `src/ui/`
   at all (`build_src_filter`).

## Non-goals (this port)

- **Touchscreen support.** CoreS3's FT6336U cap-touch is deliberately out of scope for now;
  input is CardKB-only. Touch is a clean follow-on phase later.
- **No engine changes.** `scanner/connection/gatt/adv_parser/uuid_db/hid_parser/oui_db/`
  `fingerprint/clone/mitm/sentry` compile unchanged. If this port needs to touch an engine file,
  that's a signal something's wrong — stop and re-examine.
- **No new features.** Feature parity with the Cardputer UI on the new hardware. No new screens,
  no CoreS3-only capabilities.
- **No upstream PR** right now. This is a private fork for Aaron's hardware. (Upstream is dormant;
  a PR can come later if ever.)

## Anti-goals (what makes this a failure even if it "works")

- A second **copy** of the UI code that drifts from the Cardputer version. (The whole reason we
  chose the HAL over a forked `src/ui_cores3/`.)
- Breaking or subtly changing the Cardputer build. "Without breaking the others" is the headline
  constraint — a Cardputer regression is a failed port even if CoreS3 runs great.
- Engine edits sneaking in under the guise of "the port needed it."
- GPL hygiene lost: Matthew Kukanich's copyright/`SPDX` headers and the GPL LICENSE must remain
  intact on every file, including new ones (new files get the same GPL-3.0-or-later header).

## Constraints

- **pioarduino platform** (Arduino-ESP32 3.x / ESP-IDF 5.5), shared across all envs — mixing the
  mainline espressif32 platform collides on the framework package (per the existing
  `platformio.ini` note). Keep all envs on pioarduino.
- CoreS3 LCD changed **ILI9342C → ILI9342E on 2026.8.7** and needs **M5GFX ≥ 0.2.27**; pinned to
  **M5GFX `#0.2.36` + M5Unified `#0.2.29`** (PlonkWave's proven CoreS3 pins — both cover C and E,
  so the physical unit's panel variant doesn't gate the build).
- **Port ownership for flashing:** before any `pio run -e cores3 -t upload`, no monitor/serial
  reader may hold the port. (Silabel flashes; Aaron observes the glass. HIL = his eyes.)
- CardKB protocol details (exact byte for arrows) aren't on the M5 doc page — but the port's
  nav keys are plain ASCII, so this isn't a blocker (see "left to builder").
- **Deployment target is bmorcelli's launcher** (ex-"M5 Launcher"), same as the Cardputer. This
  makes **SD the storage contract** (no runtime LittleFS) and makes the partition-table size a
  dev-path-only detail (see "Deployment" under the decided how). Aaron, 2026-11-12.

## Hardware facts (grounded, from the live M5 docs 2026-11-12)

**CoreS3:** ESP32-S3, 16 MB flash, **8 MB PSRAM**. Display ILI9342C/E, **320×240** SPI
(MOSI G37 / SCK G36 / CS G3 / DC G35). Touch FT6336U @ 0x38. PMIC AXP2101 @ 0x34, IO-expander
AW9523B @ 0x58, speaker amp AW88298 @ 0x36 — all on the **internal** I2C bus (SDA G12 / SCL G11).
microSD: **MISO G35 / MOSI G37 / SCK G36 / CS G4** (shares the LCD SPI bus). **Grove Port.A I2C:
SDA G2 / SCL G1** — a separate bus from the internal one.

**CardKB v1.1:** I2C **@ 0x5F** on Port.A. **Resolves Shift/Sym/Fn on-device** and returns plain
ASCII (confirmed on the doc page). 50-key QWERTY, ATMega8A encoder. Reading the device returns a
key byte; 0x00 = no key (standard CardKB behavior; exact control-key bytes confirmed at bring-up).

> The decisive detail: **CardKB on Port.A (G2/G1) does not touch the internal G11/G12 bus**, so it
> can't contend with the PMIC/touch/speaker. A dedicated `Wire` instance on G2/G1 reads it cleanly.

## The decided how

### Branch / repo model
- Working copy: `~/Projects/SecOps/ESP32/ChimeraBLE` (fork clone). Reference copy in
  `~/Projects/Reference/ChimeraBLE` stays pristine.
- Remotes: `origin` → `plonklab/ChimeraBLE`, `upstream` → `MatthewKuKanich/ChimeraBLE`.
- House model: create `develop` from `main` (main tracks upstream/releasable); do the port on a
  feature branch `feat/cores3-cardkb` off `develop`; PR into `develop`; **Aaron merges** (never Sil).

### The compile-time board HAL (the core of the design)
The `src/ui/` front-end currently calls the `M5Cardputer` global directly. We introduce a thin
board layer so the UI calls board-neutral names, resolved at compile time per env:

- **`src/ui/board.h`** — selected by a build flag (`-DCHIMERA_BOARD_CARDPUTER` /
  `-DCHIMERA_BOARD_CORES3`). Provides:
  - `BOARD` → the M5Unified-family global: `M5Cardputer` (cardputer) or `M5` (cores3). Because
    `M5Cardputer` *is* an `M5Unified` subclass, `BOARD.Display`, `BOARD.Speaker`, `BOARD.Power`,
    and `BOARD.update()` are **identical API on both** — these become a mechanical
    `M5Cardputer` → `BOARD` rename across `UI.cpp` / `App.cpp`.
  - **`begin()` is NOT a rename — it needs a real shim.** The current call is
    `M5Cardputer.begin(cfg, true)` (`UI.cpp:47`); that second `bool` enables the Cardputer matrix
    keyboard and is not a parameter of `M5Unified::begin(cfg)`. So `hal::begin(cfg)` branches:
    cardputer → `M5Cardputer.begin(cfg, true)`; cores3 → `M5.begin(cfg)` **then** bring up the
    CardKB `Wire` on G2/G1 (after `begin`, so the pins are settled).
  - Board constants: `BOARD_CANVAS_PSRAM` (false on Cardputer/StampS3, **true** on CoreS3's 8 MB
    PSRAM), and SD pins `BOARD_SD_{SCK,MISO,MOSI,CS}` (Cardputer 40/39/14/12 → CoreS3 36/35/37/4).
- **Keyboard** — the one true divergence, and the one place that needs careful design (the UI
  reads the keyboard in **more than just the main loop**):
  - `App.cpp` touches the keyboard at **~8 `M5Cardputer.Keyboard.*` sites across 7 functions**:
    the main `handleKeyboard()` (385-388) **plus six blocking helper loops** —
    `connectCancelRequested()` (1067, `'`'` aborts a hung connect), `popup()` (1111, any key),
    `confirmBlocking()` (1121, reads enter/del/word), the MITM pairing wait (1451), the
    switch-device disconnect waits (1093, 1402, 1433), and `chargingTick()` (1680, any key exits).
    There are **7 `M5Cardputer.update()` calls** (286, 1068, 1093, 1402, 1433, 1452, 1680). All of
    these migrate — not just `handleKeyboard()`.
  - These helpers use `isChange()/isPressed()` as an **edge query** ("did a key go down *this
    poll*"). So the HAL contract must provide the edge, not just a value:
    `hal::KeyState { std::vector<char> word; bool enter; bool del; bool changed; bool pressed; }`,
    read via `hal::readKeyboard()`.
  - Cardputer: `hal::readKeyboard()` wraps `M5Cardputer.Keyboard` (`isChange()`→`changed`,
    `isPressed()`→`pressed`, `keysState()`→word/enter/del) — **behavior-identical**.
  - CoreS3: **`src/ui/cardkb.{h,cpp}`** — a dedicated `Wire` on G2/G1, polled each `hal::update()`,
    owning a small **last-byte / edge state machine**: it latches a fresh key byte and raises
    `changed`+`pressed` for exactly one poll, so the blocking helpers see a clean down-edge and
    `popup()`/`confirmBlocking()` don't fire on a stale byte. A non-zero byte → `word=[c]`,
    `enter=(c==0x0D)`, `del=(c==0x08)`; nav `; . / , '` and command-mode text are plain ASCII and
    pass straight through (the `'` command key is 0x60 — a normal ASCII byte, no special-case). The
    CardKB↔key mapping lives entirely in the HAL so `App.cpp` stays board-neutral.
  - `hal::update()` = `BOARD.update()` (+ CardKB poll on CoreS3).
  - **Open bring-up fact (left to builder, see below):** whether the CardKB streams a held byte,
    sends once, or auto-repeats with 0x00 gaps — this tunes the edge debounce (avoid multi-advance
    on a hold, or missed repeats). The state machine is designed to be tuned once observed, not
    redesigned.

Net: `src/ui/` stays **one shared codebase**; `board.h` + `cardkb.cpp` are the only board-specific
files, picked by `build_src_filter` + the board flag. Zero runtime cost (all compile-time).

### platformio.ini — new env
```
[env:cores3]
board = m5stack-cores3
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv     ; CoreS3 is 16MB flash; use it
build_src_filter = +<*> -<main.cpp>           ; uses src/ui/, like cardputer
build_flags = ${env.build_flags} -DARDUINO_USB_CDC_ON_BOOT=1 -DCHIMERA_BOARD_CORES3
lib_deps =
    bblanchon/ArduinoJson@^7.0.0              ; from the shared engine deps (NOT NimBLE's M5)
    h2zero/NimBLE-Arduino@^2.1.0
    https://github.com/m5stack/M5GFX.git#0.2.36      ; M5GFX FIRST so M5Unified's dep is satisfied
    https://github.com/m5stack/M5Unified.git#0.2.29  ; NOT M5Cardputer; proven on PlonkWave's CoreS3
```
These M5GFX/M5Unified pins are the exact ones PlonkWave ships on real CoreS3 hardware; both are
past the 0.2.27 ILI9342E floor, so they drive the C *and* E panel without knowing which unit it is.
`lib_deps` is per-env, so this doesn't touch the `cardputer` env's `M5Cardputer` dep. The shared
`[env]` platform (`55.03.34`) is left as-is (don't perturb the other targets); if M5Unified 0.2.29
needs a newer pioarduino it'll surface at step-2 build — PlonkWave runs `55.03.312`, a known-good
fallback. The `cardputer` env gains `-DCHIMERA_BOARD_CARDPUTER`; serial envs gain nothing (no
`src/ui/`). **LittleFS stays exactly as today** — the clone/marker fallback only; no `uploadfs`/
OUI-from-flash path is in scope for this port (storage is SD-first, below).

### Display re-flow (240×135 → 320×240)
Most of `UI.cpp` already reads `canvas_.width()/height()` dynamically, so the sprite auto-sizes and
**most screens will render immediately** — just with Cardputer-tuned proportions. The real work is
per-screen tuning: more list rows now fit, fonts can grow for the bigger panel, the foxhunt bar and
header/footer want re-proportioning. Approach: get it rendering first (canvas auto-sizes), then tune
screen-by-screen on glass. The header (20px) / footer (18px) bars are fine as-is.

### Deployment: loaded by bmorcelli's launcher (app-only image)
**Primary delivery is the same as the Cardputer: bmorcelli's launcher** (formerly "M5 Launcher")
loads the app from SD. The upstream repo already does this for the Cardputer and the recipe
transfers directly:
- The launcher loads an **app-only `firmware.bin`** (standard app offset 0x10000) — *not* a merged
  image. `tools/build_release.sh` already emits `ChimeraBLE_cardputer_M5Launcher.bin` = just
  `.pio/build/cardputer/firmware.bin`. The port adds the CoreS3 equivalent
  (`ChimeraBLE_cores3_M5Launcher.bin` = `.pio/build/cores3/firmware.bin`).
- **The launcher's own partition table governs the device, not ours.** So the `[env:cores3]`
  partition CSV only affects the *direct-flash* path (the merged `0x0` image + `buildfs`), not the
  launcher path. The app just needs to be a standard 0x10000-offset image, which any of these CSVs
  produce (confirmed against Bruce's CoreS3 layout — same offset). → The 8-vs-16 MB choice is
  **low-stakes**; 16 MB stays for the dev/direct-flash path, moot under the launcher.
- **Both builds are first-class, off one compile** (mirrors upstream's Cardputer, which already
  emits both): `tools/make_merged_bin.sh` + `build_release.sh` get a `cores3` case (chip esp32s3,
  BOOT_OFF 0x0, FS_OFF per the 16 MB table) producing **`ChimeraBLE_cores3_merged.bin`** (direct
  `0x0` flash — pio/esptool, and PlonkFlash-compatible: S3 @ 0x0) **and**
  **`ChimeraBLE_cores3_M5Launcher.bin`** (app-only, launcher-loaded). Port step 6 work.
- **OUI DB is read from SD in both builds** (mirrors the Cardputer App), so there is no "which way
  was I loaded" runtime branch. The merged image may still carry a LittleFS OUI copy harmlessly; the
  app goes to SD regardless. (Opt-in enhancement, not in this port: read OUI from LittleFS on the
  direct-flash path so a direct-flashed unit needs no SD card for vendor names.)
- **Dev iteration:** Silabel flashes directly via `pio run -e cores3 -t upload` for fast cycles;
  **final HIL must also load via the launcher** (copy the app-only bin to SD → pick it) because
  that's the real runtime — and it's where the SD-only storage + the clone-reboot behavior (below)
  actually exercise.

### Storage (oui.bin / dumps / clones) — SD, because the launcher owns the flash
Under the launcher our partition table isn't active, so **LittleFS is not available at runtime** —
**all persistent data must live on SD**: `oui.bin`, clone profiles, Sentry rules, saved dumps, and
honeypot logs, under the `/ChimeraBLE/` folder, exactly as the Cardputer does today. `build_release.sh`
already states this ("the Cardputer app reads [the OUI DB] from the SD card"). The only board change
is the SD pins (`BOARD_SD_*`: CoreS3 36/35/37/4). The App already handles a missing SD / absent
LittleFS gracefully (`App.cpp:231`), so no new fallback logic is needed. (During a direct dev-flash
our partition *is* live and LittleFS would work, but we don't rely on it — **SD is the contract**.)

### Clone-advertise reboot under the launcher (flag)
`clone advertise` reboots to set the BT MAC before the stack starts (writes `/clones/.active`,
reboots, resumes). Under the launcher a plain `esp_restart()` should re-boot the *same* app (otadata
unchanged; returning to the launcher needs a boot-time button hold) → ChimeraBLE comes back with the
spoofed MAC and resumes. **This is identical to the existing Cardputer target** (also launcher-loaded),
so it's a verify-on-glass item, not a new problem this port introduces.

## What's left to the builder

- **Confirm CardKB control bytes at first plug-in** (Enter/Backspace/Esc). Standard CardKB emits
  `0x0D`/`0x08`; a 10-second check (flash, press keys, read) locks it. Arrow-key bytes are only
  needed if we later map arrows as an alternative to `;./,'` — not for this port.
- **Confirm `m5stack-cores3` + CDC-on-boot flashing** on this box's USB (native USB-Serial-JTAG).
- **Per-screen layout tuning** — judgment call per screen; the doc fixes the method (render first,
  tune on glass), not every pixel.
- **CoreS3 SD + LCD on the shared SPI bus** — a known-working M5 pattern, but verify SD mounts with
  the LCD active (the SD init runs after `BOARD.begin()`).
- **Settings/charging-mode niceties** that assumed Cardputer power behavior (e.g. "charges only
  while powered on") — verify against AXP2101 behavior; cosmetic, fix on glass.

## Build order (for the plan)
1. HAL extraction on **Cardputer only** (`board.h` + route `src/ui/` through `BOARD`/`hal::`),
   rebuild `cardputer`, confirm identical → proves "don't break the others" before CoreS3 exists.
2. `cores3` env + `cardkb.cpp` + CoreS3 `board.h` branch → first boot (splash + main menu) on glass.
3. CardKB nav end-to-end (move/open/back/command-mode) on glass.
4. Per-screen layout re-flow for 320×240.
5. SD bring-up (CoreS3 pins) + oui.bin + dumps/clones/honeypot logs (SD is the storage contract).
6. Launcher tooling: add the `cores3` case to `make_merged_bin.sh` + `build_release.sh` (app-only
   `ChimeraBLE_cores3_M5Launcher.bin` + optional merged image); load via bmorcelli's launcher on glass.
7. Full feature sweep **loaded via the launcher** (scan→connect→dump→hid→clone→foxhunt→sentry→mitm),
   since that's the real runtime (SD-only storage + clone-reboot both exercise here).

## Review notes folded in (2026-11-12, cold-eyes pre-plan review)

Calls made in response to the review (so they can be checked while reading):
- **Keyboard HAL contract widened** to carry the edge (`changed`/`pressed`), and `cardkb.cpp` owns a
  last-byte/edge state machine — because the keyboard is read in **7 blocking helper loops**, not
  just `handleKeyboard()` (the review corrected my "~5 sites" to ~8 keyboard + 7 `update()` across
  7 functions). This is step-1 (HAL) work, sized up accordingly.
- **`hal::begin()` is a real per-board shim**, not a rename — `M5Cardputer.begin(cfg, true)`'s
  keyboard-enable bool isn't an `M5Unified::begin` parameter.
- **Partition table → 16 MB** (`default_16MB.csv`, `flash_size=16MB`); LittleFS stays fallback-only;
  no `uploadfs` path in scope.
- **M5 libs pinned to M5GFX `#0.2.36` + M5Unified `#0.2.29`** (PlonkWave-proven); neutralizes the
  "which panel / which version" question.

The review confirmed the core theses: engine untouched (no `src/ui/` file reaches the engine except
through board-neutral namespaced APIs + an injected `FS*`), one shared `src/ui/`, keyboard the only
real divergence. Note the CoreS3 `DC`(G35)/SD-`MISO`(G35) dual-use is expected shared-bus behavior,
already flagged as builder-verify under "SD + LCD on the shared SPI bus."

---
*Fork of ChimeraBLE © 2026 Matthew Kukanich, GPL-3.0-or-later. CoreS3/CardKB port by plonklab.*
