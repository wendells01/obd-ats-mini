#include "Common.h"
#include "Themes.h"
#include "Menu.h"
#include "Draw.h"
#include "BleMode.h"
#include <math.h>

// Global OBD screen state
uint8_t obdScreenIdx = 0;                                   // 0=T1 (tach), 1=T2 (data)

// Runtime shift-RPM threshold — defaults to SHIFT_RPM_LIMIT, override via webUI
uint16_t obdShiftRpmLimit = SHIFT_RPM_LIMIT;
bool obdPidEnabled[OBD_PID_COUNT] = {
  true,   // 0: rpm
  true,   // 1: speed
  true,   // 2: coolantTemp
  true,   // 3: engineLoad
  true,   // 4: intakeTemp
  false,  // 5: mafRate
  true,   // 6: throttlePos
  false,  // 7: timingAdvance
  true,   // 8: fuelLevel
  true    // 9: batteryVoltage
};

// ─────────────────────────────────────────────────────────
// SHIFT! overlay — full-screen, FuelTech-style
// ─────────────────────────────────────────────────────────
static void drawObdShiftOverlay()
{
  spr.fillRect(0, 19, 320, 151, TFT_YELLOW);
  spr.drawRect(0, 19, 320, 151, TFT_RED);
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextSize(3);
  spr.setTextDatum(CC_DATUM);
  if ((millis() / 100) & 1) {
    spr.setTextColor(TFT_RED);
    for (int dx = -1; dx <= 1; dx++)
      for (int dy = -1; dy <= 1; dy++)
        spr.drawString("SHIFT!", 160 + dx, 90 + dy);
  }
  spr.setTextSize(1);
  spr.setFreeFont(NULL);
}

// ─────────────────────────────────────────────────────────
// 8-dot shift-light row (fills left-to-right as RPM rises)
// ─────────────────────────────────────────────────────────
static void drawObdShiftLight(const ObdData& d)
{
  const uint16_t DIM = spr.color565(60, 60, 60);
  const int numDots = 8;
  const int spacing = 300 / 9;  // ≈33px between centers
  const int y = 32;  // vertical center of inner frame area (y=22 to y=41)
  const int r = 8;   // dot radius (~90% of available height)

  // Colors: 1-2 yellow, 3-4 green, 5-6 red, 7-8 blue
  static const uint16_t dotColors[8] = {
    TFT_YELLOW, TFT_YELLOW,
    TFT_GREEN,  TFT_GREEN,
    TFT_RED,    TFT_RED,
    TFT_BLUE,   TFT_BLUE
  };

  for (int i = 0; i < numDots; i++) {
    int x = 10 + (i + 1) * spacing;
    uint16_t threshold = obdShiftRpmLimit * (i + 1) / numDots;
    if (d.rpm >= threshold) {
      spr.fillCircle(x, y, r, dotColors[i]);
    } else {
      spr.fillCircle(x, y, r, DIM);
    }
  }
}

// ─────────────────────────────────────────────────────────
// T1 — Lopaka-Style Digital Dashboard (adapted from 240×240 → 320×170)
// ─────────────────────────────────────────────────────────
void drawObdScreenT1(const ObdData& d)
{
  static const uint16_t LP_BG    = spr.color565(115, 117, 115);  // 0x73AE
  static const uint16_t LP_FRAME = spr.color565(58, 150, 96);    // 0x3A96
  static const uint16_t LP_GRID  = spr.color565(36, 190, 80);    // 0x24BE
  static const uint16_t LP_RED   = spr.color565(242, 6, 6);      // 0xF206
  static const uint16_t GRAY     = spr.color565(120, 120, 120);

  // ── SECTION 1: SHIFT LIGHT (y=19 to y=44) ──────────────
  spr.fillRect(0, 19, 320, 151, LP_BG);

  // Outer frame
  spr.fillRoundRect(2, 19, 316, 25, 5, LP_FRAME);
  // 8-dot shift light row
  drawObdShiftLight(d);

  // ── SECTION 2: SPEED (y=46 to y=121, left 180px) ──────
  spr.fillRoundRect(2, 46, 180, 75, 5, LP_FRAME);
  // Inner frame
  spr.fillRoundRect(5, 49, 174, 69, 5, TFT_BLACK);

  // Speed value — left-aligned, inline km/h on the right
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextSize(3);
  spr.setTextDatum(TL_DATUM);
  if (d.speedValid) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%3d", d.speed);
    spr.setTextColor(TFT_WHITE);
    spr.drawString(buf, 18, 34);
  } else {
    spr.setTextColor(GRAY);
    spr.drawString("--", 18, 34);
  }
  spr.setTextSize(1);
  spr.setFreeFont(NULL);
  spr.setTextColor(TFT_WHITE);
  spr.setTextDatum(TC_DATUM);
  spr.drawString("km/h", 90, 110, 1);

  // ── SECTION 2b: 2×2 DATA GRID (right side, y=46 to y=110) ──
  // COOL: x=202, y=46 | LOAD: x=264, y=46
  // VOLT: x=202, y=78 | FUEL: x=264, y=78
  auto drawDataBlock = [&](int x, int y, const char* label, const char* value, bool valid) {
    spr.fillRoundRect(x, y, 58, 32, 3, LP_FRAME);
    spr.fillRoundRect(x+2, y+2, 54, 28, 3, TFT_BLACK);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(TC_DATUM);
    spr.drawString(label, x + 29, y + 4, 1);
    spr.setTextSize(1);
    spr.setTextColor(valid ? TFT_WHITE : GRAY);
    spr.drawString(value, x + 29, y + 15, 2);
    spr.setTextSize(1);
    spr.setTextDatum(TL_DATUM);
  };

  // COOL
  {
    char buf[8];
    if (d.coolantTempValid) {
      snprintf(buf, sizeof(buf), "%d C", d.coolantTemp);
      drawDataBlock(202, 46, "COOL", buf, true);
    } else {
      drawDataBlock(202, 46, "COOL", "--", false);
    }
  }
  // LOAD
  {
    char buf[8];
    if (d.engineLoadValid) {
      snprintf(buf, sizeof(buf), "%u %%", d.engineLoad);
      drawDataBlock(264, 46, "LOAD", buf, true);
    } else {
      drawDataBlock(264, 46, "LOAD", "--", false);
    }
  }
  // VOLT
  {
    char buf[8];
    if (d.batteryVoltageValid) {
      snprintf(buf, sizeof(buf), "%.1fV", d.batteryVoltage);
      drawDataBlock(202, 78, "VOLT", buf, true);
    } else {
      drawDataBlock(202, 78, "VOLT", "--", false);
    }
  }
  // FUEL
  {
    char buf[8];
    if (d.fuelLevelValid) {
      snprintf(buf, sizeof(buf), "%u %%", d.fuelLevel);
      drawDataBlock(264, 78, "FUEL", buf, true);
    } else {
      drawDataBlock(264, 78, "FUEL", "--", false);
    }
  }

  // ── SECTION 3: BOTTOM ROW (y=124 to y=154) ────────────
  // 4 blocks 77w × 30h at x = 2, 81, 160, 239
  auto drawBotBlock = [&](int x, const char* label, const char* value, bool valid) {
    spr.fillRoundRect(x, 124, 77, 30, 3, LP_FRAME);
    spr.fillRoundRect(x+2, 126, 73, 26, 3, TFT_BLACK);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(TC_DATUM);
    spr.drawString(label, x + 38, 128, 1);
    spr.setTextSize(1);
    spr.setTextColor(valid ? TFT_WHITE : GRAY);
    spr.drawString(value, x + 38, 137, 2);
    spr.setTextSize(1);
    spr.setTextDatum(TL_DATUM);
  };

  // IAT
  {
    char buf[8];
    if (d.intakeTempValid) {
      snprintf(buf, sizeof(buf), "%d C", d.intakeTemp);
      drawBotBlock(2, "IAT", buf, true);
    } else {
      drawBotBlock(2, "IAT", "--", false);
    }
  }
  // THR
  {
    char buf[8];
    if (d.throttlePosValid) {
      snprintf(buf, sizeof(buf), "%u %%", d.throttlePos);
      drawBotBlock(81, "THR", buf, true);
    } else {
      drawBotBlock(81, "THR", "--", false);
    }
  }
  // MAF
  {
    char buf[8];
    if (d.mafRateValid) {
      snprintf(buf, sizeof(buf), "%.1f", d.mafRate / 100.0f);
      drawBotBlock(160, "MAF", buf, true);
    } else {
      drawBotBlock(160, "MAF", "--", false);
    }
  }
  // TIM
  {
    char buf[8];
    if (d.timingAdvanceValid) {
      snprintf(buf, sizeof(buf), "%d deg", d.timingAdvance);
      drawBotBlock(239, "TIM", buf, true);
    } else {
      drawBotBlock(239, "TIM", "--", false);
    }
  }

  // ── SECTION 4: SHIFT OVERLAY (preserved) ──────────────
  if (d.rpm >= obdShiftRpmLimit)
    drawObdShiftOverlay();
}

// ─────────────────────────────────────────────────────────
// T2 — 6-row PID data grid (WebUI-filtered)
// ─────────────────────────────────────────────────────────
void drawObdScreenT2(const ObdData& d)
{
  const uint16_t GRAY = spr.color565(120, 120, 120);

  // ── Helper: status → fill colour ──────────────────
  auto dotFill = [&](int status) -> uint16_t {
    switch (status) {
      case 0:  return TH.smeter_bar;
      case 1:  return TH.smeter_bar_plus;
      case 2:  return TH.text_warn;
      default: return GRAY;
    }
  };

  // ── Draw a 6px status dot ────────────────────────
  auto drawDot = [&](int dx, int dy, uint16_t col) {
    spr.fillCircle(dx, dy, 3, col);
  };

  // ── Draw a mini progress bar ─────────────────────
  auto drawMiniBar = [&](int bx, int by, int bw, int bh,
                          uint8_t pct, uint16_t col) {
    spr.drawRect(bx, by, bw, bh, TH.text_muted);
    if (pct > 0) {
      int fillW = (bw - 2) * pct / 100;
      if (fillW > 0)
        spr.fillRect(bx + 1, by + 1, fillW, bh - 2, col);
    }
  };

  // ── Status helpers ───────────────────────────────
  auto coolantStatus = [](int16_t v) -> int {
    if (v > 105) return 2;
    if (v > 95)  return 1;
    return 0;
  };
  auto loadStatus = [](uint8_t v) -> int {
    if (v > 85) return 2;
    if (v > 60) return 1;
    return 0;
  };
  auto battStatus = [](float v) -> int {
    if (v < 11.0f || v > 15.0f) return 2;
    if (v < 12.0f || v > 14.5f) return 1;
    return 0;
  };
  auto throttleStatus = [](uint8_t v) -> int {
    if (v > 70) return 2;
    if (v > 30) return 1;
    return 0;
  };
  auto intakeStatus = [](int16_t v) -> int {
    if (v > 60) return 2;
    if (v > 40) return 1;
    return 0;
  };

  // ── Bar-percentage helpers ───────────────────────
  auto coolantPct = [](int16_t v) -> uint8_t {
    if (v <= 40) return 0;
    if (v >= 130) return 100;
    return (uint8_t)((v - 40) * 100 / 90);
  };
  auto battPct = [](float v) -> uint8_t {
    if (v <= 10.0f) return 0;
    if (v >= 16.0f) return 100;
    return (uint8_t)((v - 10.0f) * 100 / 6.0f);
  };
  auto intakePct = [](int16_t v) -> uint8_t {
    if (v <= 0) return 0;
    if (v >= 80) return 100;
    return (uint8_t)(v * 100 / 80);
  };
  auto rpmPct = [](uint16_t v) -> uint8_t {
    if (v >= OBD_MAX_RPM) return 100;
    return (uint8_t)(v * 100 / OBD_MAX_RPM);
  };
  auto speedPct = [](uint8_t v) -> uint8_t {
    if (v >= 200) return 100;
    return (uint8_t)(v * 100 / 200);
  };
  auto fuelPct = [](uint8_t v) -> uint8_t { return v; };
  auto throttlePct = [](uint8_t v) -> uint8_t { return v; };
  auto mafPct = [](uint16_t v) -> uint8_t {
    if (v >= 600) return 100;
    return (uint8_t)(v * 100 / 600);
  };
  auto timingPct = [](int16_t v) -> uint8_t {
    if (v <= -10) return 0;
    if (v >= 40)  return 100;
    return (uint8_t)((v + 10) * 100 / 50);
  };

  // ── PID metadata ─────────────────────────────────
  struct PidRow {
    const char* label;
    bool valid;
    const char* value;      // formatted string or nullptr if invalid
    int    status;
    uint8_t barPct;
    bool   showBar;
  };

  // Collect up to 6 enabled PIDs
  PidRow rows[6];
  int rowCount = 0;
  char fmtBuf[12];

  for (int i = 0; i < OBD_PID_COUNT && rowCount < 6; i++) {
    if (!obdPidEnabled[i]) continue;

    const char* label = "";
    bool  valid = false;
    int   status = 2;
    uint8_t bpct = 0;
    bool  sbar = true;
    const char* val = nullptr;

    switch (i) {
      case 0: // RPM
        label = "RPM"; valid = d.rpmValid;
        status = (valid && d.rpm >= obdShiftRpmLimit) ? 2 : 0;
        bpct = valid ? rpmPct(d.rpm) : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%u", d.rpm); val = fmtBuf; }
        break;
      case 1: // Speed
        label = "Speed"; valid = d.speedValid;
        bpct = valid ? speedPct(d.speed) : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%u km/h", d.speed); val = fmtBuf; }
        break;
      case 2: // Coolant Temp
        label = "Cool"; valid = d.coolantTempValid;
        status = valid ? coolantStatus(d.coolantTemp) : 2;
        bpct = valid ? coolantPct(d.coolantTemp) : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%d C", d.coolantTemp); val = fmtBuf; }
        break;
      case 3: // Engine Load
        label = "Load"; valid = d.engineLoadValid;
        status = valid ? loadStatus(d.engineLoad) : 2;
        bpct = valid ? d.engineLoad : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%u %%", d.engineLoad); val = fmtBuf; }
        break;
      case 4: // Intake Temp
        label = "Intake"; valid = d.intakeTempValid;
        status = valid ? intakeStatus(d.intakeTemp) : 2;
        bpct = valid ? intakePct(d.intakeTemp) : 0;
        sbar = false;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%d C", d.intakeTemp); val = fmtBuf; }
        break;
      case 5: // MAF Rate
        label = "MAF"; valid = d.mafRateValid;
        bpct = valid ? mafPct(d.mafRate) : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%.1f g/s", d.mafRate); val = fmtBuf; }
        break;
      case 6: // Throttle Position
        label = "Throt"; valid = d.throttlePosValid;
        status = valid ? throttleStatus(d.throttlePos) : 2;
        bpct = valid ? throttlePct(d.throttlePos) : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%u %%", d.throttlePos); val = fmtBuf; }
        break;
      case 7: // Timing Advance
        label = "Timing"; valid = d.timingAdvanceValid;
        bpct = valid ? timingPct(d.timingAdvance) : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%d deg", d.timingAdvance); val = fmtBuf; }
        break;
      case 8: // Fuel Level
        label = "Fuel"; valid = d.fuelLevelValid;
        bpct = valid ? fuelPct(d.fuelLevel) : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%u %%", d.fuelLevel); val = fmtBuf; }
        break;
      case 9: // Battery Voltage
        label = "Batt"; valid = d.batteryVoltageValid;
        status = valid ? battStatus(d.batteryVoltage) : 2;
        bpct = valid ? battPct(d.batteryVoltage) : 0;
        if (valid) { snprintf(fmtBuf, sizeof(fmtBuf), "%.1fV", d.batteryVoltage); val = fmtBuf; }
        break;
    }

    rows[rowCount].label   = label;
    rows[rowCount].valid   = valid;
    rows[rowCount].value   = valid ? val : "--";
    rows[rowCount].status  = valid ? status : 2;
    rows[rowCount].barPct  = bpct;
    rows[rowCount].showBar = sbar;
    rowCount++;
  }

  // ── Render rows ──────────────────────────────────
  if (rowCount == 0) {
    spr.setTextColor(TH.text_muted);
    spr.setTextDatum(TC_DATUM);
    spr.drawString("No PIDs selected — enable in WebUI", 160, 75, 2);
    spr.setTextDatum(TL_DATUM);
  } else {
    int yStart = 22;
    int rowH   = 145 / rowCount;  // divide remaining height evenly
    if (rowH > 24) rowH = 24;    // cap at 24px for readability
    int barW = 60;
    int barH = 8;

    for (int r = 0; r < rowCount; r++) {
      int ry = yStart + r * rowH;
      uint16_t fg = rows[r].valid ? TH.text : GRAY;

      // Dot + label
      drawDot(8, ry + rowH / 2, rows[r].valid ? dotFill(rows[r].status) : GRAY);
      spr.setTextColor(fg);
      spr.setTextDatum(TL_DATUM);
      spr.drawString(rows[r].label, 16, ry + 2, 2);

      // Value
      spr.drawString(rows[r].value, 70, ry + 2, 2);

      // Mini bar
      if (rows[r].showBar && rows[r].valid) {
        int bx = 190;
        drawMiniBar(bx, ry + (rowH - barH) / 2, barW, barH,
                    rows[r].barPct,
                    rows[r].valid ? dotFill(rows[r].status) : GRAY);
      }
    }
  }

  // SHIFT overlay
  if (d.rpm >= obdShiftRpmLimit)
    drawObdShiftOverlay();
}

// ─────────────────────────────────────────────────────────
// OBD encoder navigation — toggle between T1 (0) and T2 (1)
// ─────────────────────────────────────────────────────────
bool doObdNavigation(int16_t enc)
{
  if (enc == 0)
    return false;
  obdScreenIdx = (obdScreenIdx == 0) ? 1 : 0;
  return true;
}

// ─────────────────────────────────────────────────────────
// Main OBD dispatch — shows status bar, routes to T1/T2
// ─────────────────────────────────────────────────────────
void drawLayoutObd(const char *statusLine1, const char *statusLine2)
{
  (void)statusLine1;
  (void)statusLine2;

  bool connected = BLEObd.isConnected();
  bool ready     = BLEObd.isReady();
  const ObdData& d = BLEObd.obdData();

  // ── Status bar ────────────────────────────────────────
  spr.setTextColor(TH.text);
  spr.setTextDatum(TL_DATUM);

  if (BLEObd.isDemoMode())
    spr.drawString("OBD: demo", 5, 2, 2);
  else if (!connected)
    spr.drawString("OBD: scanning...", 5, 2, 2);
  else if (!ready)
    spr.drawString("OBD: initializing ELM327...", 5, 2, 2);
  else
    spr.drawString("OBD: connected", 5, 2, 2);

  // Current screen indicator (T1 / T2)
  {
    char scr[8];
    snprintf(scr, sizeof(scr), "T%d", obdScreenIdx + 1);
    spr.setTextColor(TH.text_muted);
    spr.setTextDatum(TR_DATUM);
    spr.drawString(scr, 315, 2, 2);
    spr.setTextDatum(TL_DATUM);
  }

  // ── Divider line ──────────────────────────────────────
  spr.drawFastHLine(0, 18, 320, TH.scale_line);

  if ((!connected || !ready) && !BLEObd.isDemoMode())
  {
    spr.setTextColor(TH.text_muted);
    spr.setTextDatum(TC_DATUM);
    spr.drawString(!connected ?
      "OBD not active - Settings -> Bluetooth -> OBD" :
      "Initializing ELM327...", 160, 75, 2);
    spr.setTextDatum(TL_DATUM);
    return;
  }

  // ── Dispatch to active screen ─────────────────────────
  switch (obdScreenIdx)
  {
    case 0:  drawObdScreenT1(d); break;
    case 1:  drawObdScreenT2(d); break;
    default: obdScreenIdx = 0; drawObdScreenT1(d); break;
  }
}
