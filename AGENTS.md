# ATS Mini — Agent Instructions

## Build

**Do NOT compile locally** (no arduino-cli in dev env). Use the GitHub Action "Build Firmware":

```bash
git push          # or: git push <remote> main
# then use gh to trigger:
gh workflow run "Build Firmware" --ref main --repo <owner>/<repo>
# monitor progress in real time:
gh run watch $(gh run list --repo <owner>/<repo> --limit 1 --json databaseId --jq '.[0].databaseId') --repo <owner>/<repo>
```

Three board variants (matrix in `build.yml`):
| Variant | Profile | PSRAM | Notes |
|---------|---------|-------|-------|
| `ospi` | `esp32s3-ospi` | OPI | default/ESP32-S3 boards |
| `qspi` | `esp32s3-qspi` | QSPI | boards with QSPI PSRAM |
| `lilygo-t-embed` | `esp32s3-ospi` | OPI | `LILYGO_SI473X` flag set |

**`gh` CLI** is installed + logged in (user `wendells01`). Use it for releases, workflow dispatch, etc.

**Release job** (`release` in build.yml) breaks on forks — `artifact/` dir not found. Firmware binaries still build fine.

---

## Architecture

- **Single Arduino sketch.** Entrypoint: `ats-mini/ats-mini.ino` (~1007 lines). Headers in same dir.
- **MCU:** ESP32-S3 + SI4732. 320×170 TFT display. Encoder + button input.
- **Global state** in `Common.h` (extern vars): `currentCmd`, `currentMode`, `currentFrequency`, `currentBFO`, `volume`, `uiLayoutIdx`, `bleModeIdx`, `wifiModeIdx`, etc.
- **Renderer:** TFT_eSprite `spr` (320×170 offscreen buffer), pushed via `spr.pushSprite()`.

### Command system (Menu.h)

Commands are `uint16_t` with `0xNN00` pattern:
- `CMD_NONE` (0x0000) — VFO mode
- `CMD_FREQ` (0x0100) — frequency input
- `CMD_BAND`..`CMD_SQUELCH` (0x1000–0x1C00) — menu modes (`isMenuMode()`)
- `CMD_SETTINGS`..`CMD_WIFIMODE` (0x2000–0x2F00) — settings modes (`isSettingsMode()`)
- `CMD_ABOUT` (0x3000), `CMD_OBD` (0x3100) — overlay modes (outside both ranges)

### UI Layouts (Menu.h)
| #define    | idx | Notes |
|------------|-----|-------|
| `UI_DEFAULT` | 0 | Main layout |
| `UI_SMETER`  | 1 | S-meter layout |
| `UI_OBD`     | 2 | Kept as #define, removed from layout **selector** (`UI_LAYOUT_COUNT=2`) |

OBD is now a **CMD overlay** (app mode), not a layout. `drawLayoutObd()` is called from the CMD_OBD block in `drawScreen()`, not from the `switch(uiLayoutIdx)`.

### Flow: drawScreen()
```
if currentCmd == CMD_ABOUT → drawAbout() + pushSprite + return
if currentCmd == CMD_OBD  → drawLayoutObd() + pushSprite + return
switch(uiLayoutIdx):
  case UI_DEFAULT:  drawLayoutDefault()
  case UI_SMETER:   drawLayoutSmeter()
  case UI_OBD:      drawLayoutObd()   // legacy fallback path
draw side bar, indicators, pushSprite
```

---

## Key conventions & gotchas

### BLE
- 4 modes: `BLE_OFF`, `BLE_ADHOC`, `BLE_HID`, `BLE_OBD`
- OBD uses BLE central mode (ELM327 adapters). Service UUID 0xFFF0.
- `isReady()` now checks `demoMode_ || elmState_ == ElmState::Ready`

### OBD-II demo mode
- Toggle via web interface (`/config` page, checkbox `obddemo`), **not** radio menu
- Persisted in NVS (`prefs.putBool("obddemo", ...)`)
- Demo generates sine-wave data at 250ms intervals
- `obddemo=0` in curl must be handled explicitly: check `value() == "1"`, not just `hasParam()`

### Theme editor
Press `T` to toggle theme-editor mode (live color pick without recompiling). Press `@` to print current theme colors to serial.

### Preferences
- NVS partition `"settings"`. Versions in `Common.h`: `VER_APP`, `VER_SETTINGS`, `VER_BANDS`, `VER_MEMORIES`, `VER_STORAGE`
- Bump these when changing layout of saved data to force reset on upgrade
- `prefsRequestSave(SAVE_ALL)`, `prefsLoad()` called at boot
- `uiLayoutIdx` clamp in `ats-mini.ino`: `if (uiLayoutIdx >= UI_LAYOUT_COUNT) uiLayoutIdx = UI_DEFAULT;`

### Compile-time flags (build_opt.h or --build-property)
- `-DHALF_STEP` — EC11E encoder half-step support
- `-DLILYGO_SI473X` — LILYGO T-Embed variant (different pinout in `Common.h`)
- `-DCONFIG_ASYNC_TCP_STACK_SIZE=4096` (in `build_opt.h`)

---

## Commands (development.md)

```bash
# Arduino CLI (if available)
arduino-cli compile --clean -e -p COM_PORT -u ats-mini

# With compile flag
arduino-cli compile --build-property "compiler.cpp.extra_flags=-DHALF_STEP" --clean -e -p COM_PORT -u ats-mini
```

May need after library upgrades:
```bash
arduino-cli core update-index
arduino-cli lib update-index
```

---

## Changelog (towncrier)

Put fragments in `changelog/` named `ID.CATEGORY.md`:
```bash
uv run towncrier create --edit ID.CATEGORY.md
```
Categories: `added`, `changed`, `fixed`, `removed`, `doc`, `misc`. If no issue/PR, use `+STRING` as ID.

Build changelog:
```bash
uv run towncrier build --version X.XX
```

---

## Release process

1. Bump `VER_APP` (and `VER_SETTINGS`/`VER_BANDS`/`VER_MEMORIES` if prefs layout changed) in `Common.h`
2. `uv run towncrier build --version X.XX`
3. Commit, push, let CI build
4. `git tag -a vX.XX -m 'Version X.XX' && git push --follow-tags`
5. CI `release` job creates GitHub release (tag must match `v?.??`)

---

## Pre-commit

```bash
uv sync && uv run prek install
```
Hooks: check-yaml, end-of-file-fixer, trailing-whitespace, mixed-line-ending (LF), clang-format (manual step), actionlint.

`clang-format` style: `{BasedOnStyle: llvm, ColumnLimit: 0}`.

---

## Docs

```bash
uv run sphinx-autobuild docs/source docs/build
# opens at http://127.0.0.1:8000
```

