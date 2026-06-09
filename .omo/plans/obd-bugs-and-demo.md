# Work Plan: OBD Fixes + Demo Mode

## Objective
Fix 3 OBD visual bugs + enhance demo mode with realistic driving simulation.

## Files to Modify
| File | Changes |
|------|---------|
| `ats-mini/Draw.cpp` | `drawObdIndicator()` — reset font leak |
| `ats-mini/Layout-OBD.cpp` | `drawObdGauge()` — RPM text z-order; `drawObdPanel()` — speed offset; help text overlap |
| `ats-mini/BleObdCentral.h` | Add demo state machine enum, timers |
| `ats-mini/BleObdCentral.cpp` | Replace `sin()` demo with state-machine driving simulation |
| `ats-mini/ats-mini.ino` | Bump `VER_APP` |

---

## Task 1 — Fix OBD indicator font leak

**File**: `ats-mini/Draw.cpp`, function `drawObdIndicator()`

**Problem**: `drawString("OBD", x, y, 1)` draws with font 1, but `setFreeFont(&Orbitron_Light_24)` called later in the layout leaks state across frames — the free font isn't reset between draw cycles. When exiting menu, the "OBD" text renders with the large 24px Orbitron font at (190, 0), appearing "big and buggy."

**Fix**: Add `spr.setFreeFont(nullptr)` before `spr.drawString("OBD", ...)`.

```cpp
// Draw "OBD" text
spr.setFreeFont(nullptr);            // ← reset any leaked free font
spr.setTextColor(color);
spr.setTextDatum(TL_DATUM);
spr.drawString("OBD", x, y, 1);
```

**Acceptance**: "OBD" indicator always renders in small GLCD font (5×7) regardless of menu state.

---

## Task 2 — Fix RPM text z-order in gauge center

**File**: `ats-mini/Layout-OBD.cpp`, function `drawObdGauge()`

**Problem**: RPM text drawn at `(cx, cy - 6)` with TC_DATUM = top-center. Font is `Orbitron_Light_24` (~24px tall). Text extends from `cy-6` down to `cy+18`. The pivot dot `fillCircle(cx, cy, 4)` covers `cy-4` to `cy+4`. Although RPM text draws AFTER the pivot dot, the text has **transparent background** — the dot shows through character gaps.

**Fix**:
1. Move RPM text position from `cy - 6` to `cy - 8` (2px higher)
2. Add `spr.setTextPadding(spr.textWidth("8888", 4))` before drawing RPM text to create a solid background pad

```cpp
// Center RPM number (lines 263-274)
spr.setFreeFont(&Orbitron_Light_24);
spr.setTextColor(TH.freq_text);
spr.setTextDatum(TC_DATUM);

char rpmStr[8];
snprintf(rpmStr, sizeof(rpmStr), "%u", rpm);

// Solid background behind RPM digits prevents needle pivot showing through
spr.setTextPadding(spr.textWidth("8888", 4));
spr.drawString(rpmStr, cx, cy - 8, 4);
spr.setTextPadding(0);  // reset padding for subsequent draws

spr.setFreeFont(nullptr);
spr.setTextColor(TH.text_muted);
spr.setTextDatum(TC_DATUM);
spr.drawString("RPM", cx, cy + 20, 2);
```

**Acceptance**: RPM digits always readable; no pivot dot visible through character gaps.

---

## Task 3 — Fix speed display position

**File**: `ats-mini/Layout-OBD.cpp`, function `drawObdPanel()`

**Problem**: Speed row draws at y=22 with `Orbitron_Light_24` (~24px tall). This places text right on the divider line at y=18 — too tight. "60km/h acima dos dados."

**Fix**: Shift the speed row (Row 0) down. Change `drawString(buf, x + w / 2, y, 4)` to use `y + 4` offset:

```cpp
// Row 0: SPEED (large, centred)
{
    spr.setFreeFont(&Orbitron_Light_24);
    spr.setTextColor(d.speedValid ? TH.freq_text : GRAY);
    spr.setTextDatum(TC_DATUM);
    char buf[12];
    if (d.speedValid)
      snprintf(buf, sizeof(buf), "%u", d.speed);
    else
      snprintf(buf, sizeof(buf), "--");
    spr.drawString(buf, x + w / 2, y + 4, 4);  // ← was y, now y+4

    spr.setFreeFont(nullptr);
    spr.setTextColor(TH.text_muted);
    spr.setTextDatum(TC_DATUM);
    spr.drawString("km/h", x + w / 2, y + 30, 2);  // ← was y+26, now y+30
}
```

**Acceptance**: Speed value has clear gap (~8px) below divider line.

---

## Task 4 — Fix help text overlap with panel

**File**: `ats-mini/Layout-OBD.cpp`, function `drawLayoutObd()`

**Problem**: "Click to exit" at `(5, 150)` overlaps with the last panel row (Intake Temp at y=138, row extends to ~y=160).

**Fix**: Move "Click to exit" from y=150 to y=158:

```cpp
// ── Help text ─────────────────────────────────────────
spr.setTextColor(TH.text_muted);
spr.setTextDatum(TL_DATUM);
spr.drawString("Click to exit", 5, 158, 2);  // ← was 150
```

**Acceptance**: No text overlap between panel data and help text.

---

## Task 5 — Replace demo mode with driving simulation

### 5a — Add state machine enum to `BleObdCentral.h`

Add to the `private:` section of `BleObdCentral`:

```cpp
enum class DemoPhase : uint8_t {
    Idle,
    Accelerating,
    ShiftCoast,
    Cruising,
    Decelerating,
    Stopping,
};
DemoPhase demoPhase_ = DemoPhase::Idle;
uint32_t demoPhaseStartMs_ = 0;
```

### 5b — Implement driving simulation in `BleObdCentral::update()`

Replace the `sin()` demo block with a state machine:

| Phase | Duration | RPM | Speed | Coolant | Load | Throttle |
|-------|----------|-----|-------|---------|------|----------|
| Idle | 3s | 800→900 | 0 | 87→88 | 15→20 | 0→5 |
| Accelerating | 8s | 900→6500 | 0→120 | 88→95 | 20→80 | 5→60 |
| ShiftCoast | 1.5s | 6500→4000 | 120→125 | 95→96 | 80→40 | 60→10 |
| Accelerating (2nd) | 4s | 4000→6500 | 125→160 | 96→102 | 40→85 | 10→65 |
| ShiftCoast (2nd) | 1.5s | 6500→4000 | 160→165 | 102→103 | 85→45 | 65→12 |
| Cruising | 6s | 4000→2200 | 165→100 | 103→97 | 45→30 | 12→18 |
| Decelerating | 5s | 2200→800 | 100→5 | 97→92 | 30→10 | 18→2 |
| Stopping | 2s | 800→0 | 5→0 | 92→90 | 10→5 | 2→0 |
| → back to Idle |

Each phase computes values by linear interpolation between start and end values based on `(now - phaseStartMs) / duration`.

### 5c — Update `enableDemoMode()`

Reset `demoPhase_` to `Idle` and `demoPhaseStartMs_` to `millis()` when demo is enabled.

---

## Task 6 — Bump version

**File**: `ats-mini/Common.h`
```cpp
#define VER_APP        236  // ← increment
```

---

## Parallelization

Tasks can be executed in this order:
1. **Task 1** (indicator font fix) — independent, do first
2. **Tasks 2+3+4** (OBD layout fixes) — can be done together
3. **Task 5** (demo mode) — can be done in parallel with 2+3+4
4. **Task 6** (version bump) — last

---

## Acceptance Criteria (all tasks)

1. [~] OBD indicator "OBD" always renders in small font, never large/buggy *(visual — testar no hardware)*
2. [~] RPM center digits fully readable, no needle pivot showing through *(visual — testar no hardware)*
3. [~] Speed value clearly separated from divider line *(visual — testar no hardware)*
4. [~] "Click to exit" text does not overlap with panel data *(visual — testar no hardware)*
5. [~] Demo mode cycles through driving phases: idle → accel → shift → cruise → decel → stop *(visual — testar no hardware)*
6. [~] RPM reaches ≥6000 during accel phases (tests SHIFT indicator) *(verificar no demo mode)*
7. [~] All data fields (speed, coolant, load, throttle) vary realistically during cycle *(visual)*
8. [x] Build compiles cleanly (all 3 variants)

---

## Notes

- Building must use GitHub Actions: `git push && gh workflow run "Build Firmware" --ref main`
- After build completes, user can flash the binary to hardware for testing
- No OBD-II scanner required to test — demo mode provides visual verification
