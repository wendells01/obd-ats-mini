#include "Common.h"
#include "Themes.h"
#include "Menu.h"
#include "Draw.h"
#include "BleMode.h"
#include <math.h>

// Global OBD screen state
uint8_t obdScreenIdx = 0;                                   // 0=T1 (tach), 1=T2 (data)
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
// Centered tachometer arc — BMW M3 G80 digital cluster style
// ─────────────────────────────────────────────────────────
static void drawObdGauge(int cx, int cy, int outerR, int innerR, uint16_t rpm)
{
  if (rpm > OBD_MAX_RPM) rpm = OBD_MAX_RPM;

  //
  // Arc track + colored zones
  //
  // TFT_eSPI angles: 0° = 3 o'clock, clockwise
  // RPM → angle: 0→135° (bottom-left), 4000→270° (top), 8000→405°/45° (bottom-right)
  // Green  135°–270° =  0–4000 RPM
  // Yellow 270°–360° = 4000–6000 RPM
  // Red    360°–405° = 6000–8000 RPM
  //
  spr.drawSmoothArc(cx, cy, outerR, innerR, 135, 405, TH.smeter_bar_empty, TH.bg);
  spr.drawSmoothArc(cx, cy, outerR, innerR, 135, 270, TH.smeter_bar, TH.smeter_bar_empty);
  spr.drawSmoothArc(cx, cy, outerR, innerR, 270, 360, TH.smeter_bar_plus, TH.smeter_bar_empty);
  spr.drawSmoothArc(cx, cy, outerR, innerR, 360, 405, TH.text_warn, TH.smeter_bar_empty);

  //
  // Tick marks at every 500 RPM (major at 1000, minor at 500)
  //
  for (unsigned int r = 0; r <= 8000; r += 500)
  {
    int  angle = 135 + (int)(r * 270u / OBD_MAX_RPM);
    float rad  = angle * (PI / 180.0f);
    float c    = cosf(rad);
    float s    = sinf(rad);

    bool major = (r % 1000 == 0);
    int  tLen  = major ? 10 : 5;
    int  x1 = cx + (int)((outerR - tLen) * c);
    int  y1 = cy + (int)((outerR - tLen) * s);
    int  x2 = cx + (int)((outerR - 1) * c);
    int  y2 = cy + (int)((outerR - 1) * s);
    spr.drawLine(x1, y1, x2, y2, major ? TH.text : TH.text_muted);

    // RPM label at major ticks (every 1000 RPM)
    if (major)
    {
      int  lr  = outerR + 7;
      int  lx  = cx + (int)(lr * c);
      int  ly  = cy + (int)(lr * s);
      char lbl[4];
      snprintf(lbl, sizeof(lbl), "%u", r / 1000);
      spr.setTextDatum(CC_DATUM);
      spr.setTextColor(TH.text_muted);
      spr.drawString(lbl, lx, ly, 1);
    }
  }

  //
  // Needle — thin elegant line + small triangle tip
  //
  float ndlAngle = 135.0f + (rpm * 270.0f) / OBD_MAX_RPM;
  float rad_     = ndlAngle * (PI / 180.0f);
  float ndlC     = cosf(rad_);
  float ndlS     = sinf(rad_);

  // Main needle line (thin, from center to near edge)
  int tipX = cx + (int)((outerR - 3) * ndlC);
  int tipY = cy + (int)((outerR - 3) * ndlS);
  spr.drawLine(cx, cy, tipX, tipY, TH.freq_text);

  // Small triangle at tip for precision
  int  btR   = outerR - 12;
  int  bX    = cx + (int)(btR * ndlC);
  int  bY    = cy + (int)(btR * ndlS);
  float perpC = cosf(rad_ + PI / 2.0f);
  float perpS = sinf(rad_ + PI / 2.0f);
  spr.fillTriangle(tipX, tipY,
                   bX + (int)(3 * perpC), bY + (int)(3 * perpS),
                   bX - (int)(3 * perpC), bY - (int)(3 * perpS),
                   TH.freq_text);

  // Small pivot dot
  spr.fillCircle(cx, cy, 3, TH.freq_text);
}

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
// T1 — BMW M3 G80 Digital Cluster (centered arc + speed inside)
// ─────────────────────────────────────────────────────────
void drawObdScreenT1(const ObdData& d)
{
  const uint16_t GRAY = spr.color565(120, 120, 120);

  // Centered tachometer arc
  drawObdGauge(160, 87, 70, 54, d.rpm);

  // Speed display — INSIDE the arc, centered
  {
    // Clear a circular area behind the readout so arc/needle don't show through
    spr.fillCircle(160, 86, 36, TH.bg);

    // RPM number (small, above speed)
    {
      char rs[8];
      snprintf(rs, sizeof(rs), "%u", d.rpm);
      spr.setFreeFont(&Orbitron_Light_24);
      spr.setTextColor(TH.text_muted, TH.bg);
      spr.setTextDatum(TC_DATUM);
      spr.drawString(rs, 160, 58);
      spr.setFreeFont(NULL);
    }

    // Speed value (large, centered)
    {
      char buf[8];
      if (d.speedValid)
        snprintf(buf, sizeof(buf), "%3d", d.speed);
      else
        snprintf(buf, sizeof(buf), "--");
      spr.setFreeFont(&Orbitron_Light_24);
      spr.setTextSize(2);
      spr.setTextColor(d.speedValid ? TH.freq_text : GRAY, TH.bg);
      spr.setTextDatum(TC_DATUM);
      spr.drawString(buf, 160, 88);
      spr.setTextSize(1);
      spr.setFreeFont(NULL);
    }

    // km/h label (below speed)
    spr.setTextColor(TH.text_muted, TH.bg);
    spr.setTextDatum(TC_DATUM);
    spr.drawString("km/h", 160, 114, 1);
  }

  // SHIFT overlay
  if (d.rpm >= SHIFT_RPM_LIMIT)
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
        status = (valid && d.rpm >= SHIFT_RPM_LIMIT) ? 2 : 0;
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
  if (d.rpm >= SHIFT_RPM_LIMIT)
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
