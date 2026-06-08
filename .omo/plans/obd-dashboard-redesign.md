# OBD Dashboard Redesign

## TL;DR

> **Quick Summary**: Replace the basic OBD overlay with an automotive-style dashboard featuring a tachometer arc-gauge, a data panel with visual indicators, and a FuelTech-style "SHIFT!" shift-light.
>
> **Deliverables**:
> - RPM arc gauge with colored zones and needle
> - Data panel with colored status indicators + mini bars
> - LED shift-light bars + "SHIFT!" blink overlay
> - Bug fix: OBD indicator overlapping band text
>
> **Estimated Effort**: Medium
> **Parallel Execution**: YES — 2 waves
> **Critical Path**: Bug fix + gauge function → data panel → SHIFT! → QA

---

## Context

### Original Request
User wants: (1) premium OBD dashboard with tachometer gauge, styled data panel, automotive look; (2) fix bug where red "OBD" text overlaps "VHF" band text on main screen.

### Interview Summary
**Key Decisions**:
- Layout: Gauge (left, 0-160px) + data panel with indicators (right, 160-320px)
- SHIFT! shift-light: LED bars + blinking overlay at RPM limit
- SHIFT! limit: customizable via `#define SHIFT_RPM_LIMIT`
- Temperature alert overlap: removed (replaced by SHIFT!)
- Data: show all 6 polled PIDs, invalid → "--"
- No web config for field selection (keep lightweight)

**Research Findings**:
- `drawSmoothArc()` already used in Menu.cpp/Draw.cpp — confirmed working on sprites
- `fillTriangle()`, `fillCircle()`, `drawSmoothRoundRect()` — all in codebase
- Orbitron_Light_24 + TFT_eSPI fonts 1/2/4/7 — only fonts available
- TFT_eSPI 2.5.43 has all needed primitives

### Metis Review
**Identified Gaps** (addressed):
- Gap: x=184 may still clip long band names → fixed to x=190
- Gap: temperature alert needs explicit handling → removed, replaced by SHIFT!
- Gap: gauge needle math needs RPM range → fixed max at 8000 RPM via #define

---

## Work Objectives

### Core Objective
Convert the OBD overlay into an automotive dashboard with a tachometer arc-gauge (left) and a data indicator panel (right), using only TFT_eSPI primitives, with zero changes to BLE/ELM327/drawScreen() flow, preserving the existing RPM mini-widget.

### Concrete Deliverables
- `Layout-OBD.cpp` — full rewrite of dashboard layout
- `Draw.cpp` — `drawObdIndicator()` coordinate fix
- `Menu.h` — optional, add `SHIFT_RPM_LIMIT` #define

### Definition of Done
- [ ] Bug fix: "OBD" text at x=190 in both Layout-Default.cpp and Layout-SMeter.cpp
- [ ] RPM gauge draws arc with colored zones + needle pointing to correct RPM
- [ ] Data panel shows 6 PIDs with colored status dots + mini bars for Load/Throttle
- [ ] SHIFT! LED bars + blinking overlay when RPM > limit
- [ ] All connecting/disconnected/demo states handled gracefully
- [ ] Firmware compiles for all 3 variants (ospi, qspi, lilygo-t-embed)

### Must Have
- Fix `drawObdIndicator(116,0)` → `drawObdIndicator(190,0)` in both layout files
- RPM gauge: `drawSmoothArc` for colored zones, `fillTriangle` for needle, Orbitron font for center value
- Data panel: colored status dots (fillCircle) + mini bars (fillRect) for Load/Throttle
- SHIFT! overlay blinking when RPM > SHIFT_RPM_LIMIT
- Respect all `*Valid` booleans — "--" when invalid
- "Click to exit" preserved for connecting/disconnected states

### Must NOT Have (Guardrails)
- NO changes to `BleObdCentral.cpp`/`.h` (parser, PIDs, BLE stack)
- NO new font files, image assets, or external libraries
- NO changes to `UI_LAYOUT_COUNT`, `Menu.h` command enum, or `drawScreen()` flow
- NO changes to `ObdData` struct or new PID parsers
- NO gauge sweep animation or interpolation (static needle update)
- NO web config for field selection (keep firmware light)
- NO breaking the mini RPM widget on Layout-Default.cpp

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed using Bash (firmware build) and code inspection/static analysis.
> Acceptance criteria requiring "user manually tests/confirms" are FORBIDDEN.

### Test Decision
- **Infrastructure exists**: NO (embedded Arduino)
- **Automated tests**: None (no test framework for Arduino)
- **QA method**: Static code review + build verification + visual layout analysis

### QA Policy
Every task includes agent-executed verification:
- **Build**: `gh workflow run "Build Firmware"` confirms compilation for all 3 variants
- **Bug fix**: `grep` + code read confirms coordinate change in both files
- **Gauge/panel**: AST-grep confirms function calls use correct primitives
- **No regression**: `git diff -- ats-mini/BleObdCentral.*` confirms zero changes to BLE stack

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Parallel — independent tasks):
├── Task 1: Fix OBD indicator coordinate bug [quick]
├── Task 2: RPM arc gauge function [visual-engineering]
└── Task 3: Data panel with indicators [visual-engineering]

Wave 2 (Sequential — depends on gauge + panel):
├── Task 4: SHIFT! LED bars + overlay [visual-engineering]
└── Task 5: Polish states + demo mode [visual-engineering]

Wave FINAL (Parallel verification, then user ok):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Visual QA + build verification (unspecified-high)
└── Task F4: Scope fidelity check (deep)
```

### Dependency Matrix
- **1**: None
- **2**: None
- **3**: None
- **4**: 2 (needs gauge arc)
- **5**: 2, 3 (needs both panels)
- **F1-F4**: 1-5 (all impl tasks)

### Agent Dispatch Summary
- **Wave 1**: 3 parallel — T1 `quick`, T2 `visual-engineering`, T3 `visual-engineering`
- **Wave 2**: 2 sequential — T4 `visual-engineering`, T5 `visual-engineering`
- **Wave FINAL**: 4 parallel — F1 `oracle`, F2 `unspecified-high`, F3 `unspecified-high`, F4 `deep`

---

## TODOs

- [ ] 1. Fix OBD indicator coordinate overlap

  **What to do**:
  - Change `drawObdIndicator` x-coordinate from `116` to `190` in both layout files
  - Files: `Layout-Default.cpp:17`, `Layout-SMeter.cpp:162`
  - The OBD text at x=190 sits safely in the gap between band text max extent (~180 with longest name) and WiFi icon (x=237)

  **Must NOT do**:
  - Do NOT change the function signature or behavior of `drawObdIndicator()`
  - Do NOT modify Draw.cpp

  **Recommended Agent Profile**:
  - Category: `quick`
  - Skills: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1
  - Blocks: None
  - Blocked By: None

  **Acceptance Criteria**:
  - [ ] `grep "drawObdIndicator" Layout-Default.cpp` shows `drawObdIndicator(190, 0);`
  - [ ] `grep "drawObdIndicator" Layout-SMeter.cpp` shows `drawObdIndicator(190, 0);`
  - [ ] No other changes in git diff outside the two line changes

  **QA Scenarios**:
  ```
  Scenario: Verify coordinate fix in both layout files
    Tool: Bash (grep)
    Preconditions: Files Layout-Default.cpp and Layout-SMeter.cpp exist
    Steps:
      1. grep -n "drawObdIndicator" ats-mini/Layout-Default.cpp → confirms x=190
      2. grep -n "drawObdIndicator" ats-mini/Layout-SMeter.cpp → confirms x=190
      3. git diff -- ats-mini/Layout-Default.cpp ats-mini/Layout-SMeter.cpp → only x-coordinate changed
    Expected Result: Both files show `drawObdIndicator(190, 0);`
    Evidence: .omo/evidence/task-1-coord-fix.txt

  Scenario: Verify no unintended changes
    Tool: Bash (git diff)
    Preconditions: Changes staged or committed
    Steps:
      1. git diff --name-only → only Layout-Default.cpp and Layout-SMeter.cpp
      2. git diff ats-mini/BleObdCentral.cpp → empty (no BLE changes)
    Expected Result: Zero changes outside the two layout files
    Evidence: .omo/evidence/task-1-no-regression.txt
  ```

  **Commit**: YES
  - Message: `fix(obd): move OBD indicator to x=190 to avoid text overlap with band`
  - Files: `ats-mini/Layout-Default.cpp`, `ats-mini/Layout-SMeter.cpp`
  - Pre-commit: N/A (no test suite)

---

- [ ] 2. Build RPM arc gauge

  **What to do**:
  - Create a static helper function `drawObdGauge(int x, int y, int r, uint16_t rpm)` in `Layout-OBD.cpp`
  - Draw a semi-circular arc using `drawSmoothArc()` with colored zones:
    - Green zone (0-4000 RPM): 150° to 240°
    - Yellow zone (4000-6000 RPM): 240° to 300°
    - Red zone (6000-8000 RPM): 300° to 350°
  - Draw the gauge arc center at approximately (82, 95), outer radius ~65, inner ~50
  - Draw tick marks at regular intervals using `drawFastHLine()`/`fillTriangle()`
  - Draw needle using `fillTriangle()` pointing to angle corresponding to current RPM
  - Draw RPM value in `Orbitron_Light_24` at gauge center
  - Draw "RPM ×1000" label below center value
  - Max RPM is 8000 (use `OBD_MAX_RPM` #define)
  - Use colors from existing theme system (TH.*) for maximum theme compatibility

  **Reference**: Menu.cpp lines 1178-1202 for `drawSmoothArc` pattern, Draw.cpp lines 65-67 for arc usage

  **Must NOT do**:
  - Do NOT add animation/interpolation (needle jumps to value directly)
  - Do NOT use any primitives beyond drawSmoothArc, fillTriangle, drawCircle, fillCircle, drawString
  - Do NOT add new #include or library dependencies

  **Recommended Agent Profile**:
  - Category: `visual-engineering`
  - Skills: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1 (with T3)
  - Blocks: Task 4 (SHIFT! needs gauge arc)
  - Blocked By: None

  **Acceptance Criteria**:
  - [ ] Function `drawObdGauge()` exists in Layout-OBD.cpp
  - [ ] Uses `drawSmoothArc` with at least 3 color zones (green/yellow/red)
  - [ ] Needle uses `fillTriangle` pointing to correct angle based on RPM value
  - [ ] Center shows RPM number in Orbitron_Light_24
  - [ ] All colors reference TH.* constants (not hardcoded RGB)

  **QA Scenarios**:
  ```
  Scenario: Verify gauge function structure
    Tool: Bash (grep + ast-grep)
    Preconditions: Layout-OBD.cpp contains drawObdGauge function
    Steps:
      1. ast_grep_search pattern "drawSmoothArc(" in Layout-OBD.cpp → confirms arc usage
      2. ast_grep_search pattern "fillTriangle(" in Layout-OBD.cpp → confirms needle
      3. ast_grep_search pattern "Orbitron_Light_24" in Layout-OBD.cpp → confirms font
      4. grep "OBD_MAX_RPM" Layout-OBD.cpp or Menu.h → confirms max RPM define
    Expected Result: All draw primitives present and correct
    Evidence: .omo/evidence/task-2-gauge-structure.txt
  ```

  **Commit**: NO (groups with T3, T4, T5)

---

- [ ] 3. Build data panel with visual indicators

  **What to do**:
  - Create a static helper function `drawObdPanel(int x, int y, const ObdData& d)` in `Layout-OBD.cpp`
  - Position: x=165 to x=320 (right half)
  - Display the following fields as compact indicator rows:
    - **SPEED**: large number in Orbitron_Light_24 at top of panel, with "km/h" label
    - **Coolant Temp**: colored dot (fillCircle) + value + unit
      - Dot green (<90°C), yellow (90-100°C), red (>100°C)
    - **Engine Load**: colored dot + value + mini progress bar (fillRect, ~40px wide)
    - **Battery Voltage**: colored dot + value
      - Dot green (>12V), yellow (11-12V), red (<11V)
    - **Throttle Position**: colored dot + value + mini progress bar
    - **Intake Temp**: colored dot + value
  - Each row: ~20px height, compact font (font 2 or 1)
  - If a field's `*Valid` boolean is false, show "--" instead of value, gray dot

  **Must NOT do**:
  - Do NOT add new data parsing or PID requests
  - Do NOT use images or icons — only primitives (fillCircle, fillRect)
  - Do NOT change the layout dimensions during runtime (no responsive logic)

  **Recommended Agent Profile**:
  - Category: `visual-engineering`
  - Skills: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1 (with T2)
  - Blocks: Task 5 (polish needs panel)
  - Blocked By: None

  **Acceptance Criteria**:
  - [ ] Function `drawObdPanel()` exists in Layout-OBD.cpp
  - [ ] Speed shown as large number with Orbitron font
  - [ ] At least 5 data fields displayed with colored status dots
  - [ ] Engine Load and Throttle have mini progress bars
  - [ ] Invalid fields show "--" with gray indicator
  - [ ] All `*Valid` checks from ObdData are respected

  **QA Scenarios**:
  ```
  Scenario: Verify panel structure
    Tool: Bash (grep + code read)
    Preconditions: Layout-OBD.cpp contains drawObdPanel function
    Steps:
      1. ast_grep_search pattern "fillCircle(" in Layout-OBD.cpp → confirms status dots
      2. ast_grep_search pattern "fillRect(" in Layout-OBD.cpp → confirms mini bars
      3. grep "Valid" Layout-OBD.cpp → confirms validity checks
      4. grep '"--"' Layout-OBD.cpp → confirms invalid state handling
    Expected Result: All panel elements present
    Evidence: .omo/evidence/task-3-panel-structure.txt
  ```

  **Commit**: NO (groups with T2, T4, T5)

---

- [ ] 4. Implement SHIFT! shift-light

  **What to do**:
  - Add `#define SHIFT_RPM_LIMIT 6000` in `Menu.h` (near the command defines)
  - In the gauge function, draw LED shift bars at the bottom of the arc:
    - 5-8 small rectangles (fillRect) below the gauge arc, not overlapping the panel
    - Each bar lights up progressively: 75%, 80%, 85%, 90%, 95%, 100% of limit
    - Colors: green → yellow → orange → red (progressive)
  - When RPM >= SHIFT_RPM_LIMIT:
    - Flash "SHIFT!" text in the center of the screen (blinking at 250ms interval)
    - Use large font (Orbitron or font 7) with red fillRect background
  - SHIFT! overlay does NOT block data panel — both visible simultaneously

  **Must NOT do**:
  - Do NOT add sound or vibration (not available on this hardware)
  - Do NOT make the SHIFT! overlay block user input (keep "Click to exit" functional)
  - Do NOT use millis() blocking delays (use the existing blink pattern with `millis() & 0x...`)

  **Recommended Agent Profile**:
  - Category: `visual-engineering`
  - Skills: `[]`

  **Parallelization**:
  - Can Run In Parallel: NO
  - Parallel Group: Wave 2 (sequential, after T2)
  - Blocks: Task 5
  - Blocked By: Task 2 (needs gauge arc to attach LED bars)

  **Acceptance Criteria**:
  - [ ] `#define SHIFT_RPM_LIMIT 6000` in Menu.h
  - [ ] LED bars drawn when RPM > 75% of limit, progressive lighting
  - [ ] "SHIFT!" text blinks when RPM >= limit
  - [ ] LED bars and SHIFT! overlay respect existing `isReady()` state
  - [ ] SHIFT! does not block "Click to exit"

  **QA Scenarios**:
  ```
  Scenario: Verify SHIFT! implementation
    Tool: Bash (grep + code read)
    Preconditions: Layout-OBD.cpp modified
    Steps:
      1. grep "SHIFT_RPM_LIMIT" Menu.h → confirms #define
      2. grep "fillRect" in gauge section → confirms LED bars
      3. grep "SHIFT" Layout-OBD.cpp → confirms overlay text
      4. grep "millis() & 0x" → confirms non-blocking blink
    Expected Result: SHIFT! components present and correct
    Evidence: .omo/evidence/task-4-shift.txt
  ```

  **Commit**: NO (groups with T2, T3, T5)

---

- [ ] 5. Polish states + wire into drawLayoutObd

  **What to do**:
  - Rewrite `drawLayoutObd()` to call the new gauge + panel functions
  - Handle all states:
    - **Demo mode**: show gauge + panel with demo data, status "OBD: demo"
    - **Connecting** (BLE started, not connected): show scanning text + "Click to exit"
    - **Connected + initializing ELM327**: show "Initializing ELM327..." + "Click to exit"
    - **Connected + ready**: show full gauge + panel + data
  - Status bar: "OBD: [state]" at top-left, "Click to exit" at bottom
  - Remove the old `drawObdBar()` static function (replaced by panel)
  - Remove the temperature alert overlay block (lines 158-183 in current Layout-OBD.cpp)
  - Update the mini RPM widget in Layout-Default.cpp if needed (still at (5, 135))
  - Test with demo mode enabled: `obddemo=1` via web interface

  **Must NOT do**:
  - Do NOT change `drawScreen()` CMD_OBD early-return pattern (still at Draw.cpp:479)
  - Do NOT change the overlay click-exit behavior (clickHandler fallthrough)
  - Do NOT modify `drawSideBar()` or other shared UI functions

  **Recommended Agent Profile**:
  - Category: `visual-engineering`
  - Skills: `[]`

  **Parallelization**:
  - Can Run In Parallel: NO
  - Parallel Group: Wave 2 (final assembly)
  - Blocks: F1-F4
  - Blocked By: Task 2, 3, 4

  **Acceptance Criteria**:
  - [ ] `drawLayoutObd()` calls `drawObdGauge()` + `drawObdPanel()` when ready
  - [ ] Connecting/disconnected states show appropriate text + "Click to exit"
  - [ ] Demo mode shows gauge + panel with demo data
  - [ ] Old `drawObdBar()` removed
  - [ ] Temperature alert block removed
  - [ ] Compiles for all 3 variants via GitHub Action

  **QA Scenarios**:
  ```
  Scenario: Verify state handling
    Tool: Bash (grep + code read)
    Preconditions: Layout-OBD.cpp fully rewritten
    Steps:
      1. grep "drawObdGauge\|drawObdPanel" Layout-OBD.cpp → confirms new calls
      2. grep "OBD: scanning\|OBD: demo\|OBD: connected" → confirms all status texts
      3. grep "Click to exit" Layout-OBD.cpp → confirms preserved
      4. grep "drawObdBar" Layout-OBD.cpp → should NOT exist (removed)
      5. grep "drawLayoutObd" Draw.cpp → confirms still the same call pattern
    Expected Result: All states handled, no regression
    Evidence: .omo/evidence/task-5-states.txt

  Scenario: Verify compilation
    Tool: Bash (gh workflow run)
    Preconditions: All changes committed, pushed to remote
    Steps:
      1. git push <remote> main
      2. gh workflow run "Build Firmware" --ref main --repo <owner>/<repo>
      3. gh run watch --exit-status (wait for completion)
    Expected Result: All 3 variants compile successfully
    Evidence: .omo/evidence/task-5-build.txt
  ```

  **Commit**: YES (with T2, T3, T4)
  - Message: `feat(obd): automotive dashboard redesign with tachometer gauge, data panel, and SHIFT! shift-light`
  - Files: `ats-mini/Layout-OBD.cpp`, `ats-mini/Menu.h` (SHIFT_RPM_LIMIT)
  - Pre-commit: `git diff -- ats-mini/BleObdCentral.*` (confirm zero changes)

---

## Final Verification Wave

- [ ] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, check function signatures). For each "Must NOT Have": search codebase for forbidden patterns. Check evidence files exist in .omo/evidence/. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [ ] F2. **Code Quality Review** — `unspecified-high`
  Review all changed files for: dead code (old drawObdBar still present), unused imports, hardcoded colors (not using TH.*), magic numbers, consistency with existing patterns. Check gauge math: angle-to-RPM conversion is correct.
  Output: `Files [N clean/N issues] | Gauge math [PASS/FAIL] | VERDICT`

- [ ] F3. **Visual QA + Build Verification** — `unspecified-high`
  Build firmware for all 3 variants via GitHub Action. Review visual output: gauge arc zones, needle position, data panel layout, SHIFT! overlay. Verify all 5 states (demo, disconnected, connecting, connected-init, ready) produce valid output.
  Output: `Build [PASS/FAIL] | Visual [PASS/FAIL] | States [N/N] | VERDICT`

- [ ] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (git log/diff). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance. Detect cross-task contamination (e.g., task touching BleObdCentral).
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | VERDICT`

---

## Commit Strategy

- **1**: `fix(obd): move OBD indicator to x=190 to avoid text overlap with band` — Layout-Default.cpp, Layout-SMeter.cpp
- **2-5 (grouped)**: `feat(obd): automotive dashboard redesign with tachometer gauge, data panel, and SHIFT! shift-light` — Layout-OBD.cpp, Menu.h

---

## Success Criteria

### Verification Commands
```bash
# Verify bug fix
grep -n "drawObdIndicator" ats-mini/Layout-Default.cpp ats-mini/Layout-SMeter.cpp
# Expected: both show drawObdIndicator(190, 0);

# Verify gauge uses correct primitives
grep -n "drawSmoothArc\|fillTriangle\|Orbitron" ats-mini/Layout-OBD.cpp

# Verify no BLE contamination
git diff -- ats-mini/BleObdCentral.cpp ats-mini/BleObdCentral.h

# Verify compilation (via GitHub Action)
gh workflow run "Build Firmware" --ref main --repo wendells01/obd-ats-mini
```

### Final Checklist
- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent
- [ ] Build passes for all 3 variants
- [ ] Bug fix confirmed in both layout files
- [ ] SHIFT_RPM_LIMIT defined in Menu.h
- [ ] No changes to BleObdCentral
