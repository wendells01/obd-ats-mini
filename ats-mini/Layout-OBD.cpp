#include "Common.h"
#include "Themes.h"
#include "Menu.h"
#include "Draw.h"
#include "BleMode.h"
#include <math.h>

// ─────────────────────────────────────────────────────────
// Right-side OBD data panel — 6 PID indicators
// ─────────────────────────────────────────────────────────
static void drawObdPanel(int x, int y, int w, const ObdData& d)
{
  const uint16_t GRAY = spr.color565(120, 120, 120);

  // ── Row 0: SPEED (large, left-aligned, inline km/h) ─
  {
    spr.setFreeFont(&Orbitron_Light_24);
    spr.setTextColor(d.speedValid ? TH.freq_text : GRAY);
    spr.setTextDatum(TL_DATUM);
    char buf[16];
    if (d.speedValid)
      snprintf(buf, sizeof(buf), "%u km/h", d.speed);
    else
      snprintf(buf, sizeof(buf), "-- km/h");
    spr.drawString(buf, x, y, 4);
    spr.setFreeFont(NULL);
  }

  // ── Helper: status → fill colour ──────────────────
  auto dotFill = [&](int status) -> uint16_t {
    switch (status) {
      case 0:  return TH.smeter_bar;       // green
      case 1:  return TH.smeter_bar_plus;   // yellow
      case 2:  return TH.text_warn;         // red
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

  // ── Draw one data row (dot + label + value + bar) ─
  auto drawRow = [&](int ry, const char* label, const char* val,
                     bool valid, int status, int barPct, bool showBar) {
    uint16_t fg   = valid ? TH.text : GRAY;
    uint16_t dcol = valid ? dotFill(status) : GRAY;

    // Status dot (6px diameter, centred in 22px row)
    drawDot(x + 4, ry + 11, dcol);

    spr.setTextColor(fg);
    spr.setTextDatum(TL_DATUM);
    spr.drawString(label, x + 10, ry + 2, 2);

    spr.drawString(val, x + 52, ry + 2, 2);

    if (showBar && valid) {
      int bw = w - 96;  // ends at x+w-6 (6px right margin)
      if (bw > 10)
        drawMiniBar(x + 90, ry + 7, bw, 8, barPct, dcol);
    }
  };

  // ── Status helpers ───────────────────────────────
  // Returns 0=green, 1=yellow, 2=red
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

  // ── Data rows ───────────────────────────────────
  char buf[16];
  uint16_t fg;

  // Row 1 (y=50) — Coolant Temp
  {
    bool ok = d.coolantTempValid;
    fg = ok ? TH.text : GRAY;
    // drawRow without the value so we can append °C manually
    drawRow(50, "Cool", "", ok,
            ok ? coolantStatus(d.coolantTemp) : 2,
            ok ? coolantPct(d.coolantTemp) : 0, true);
    // Redraw value + degree symbol manually (font 2 has no ° glyph)
    spr.setTextColor(fg);
    if (ok) {
      char val[8];
      snprintf(val, sizeof(val), "%d", d.coolantTemp);
      int vx = x + 52, vy = 50 + 2;
      spr.drawString(val, vx, vy, 2);
      int valW = spr.textWidth(val, 2);
      spr.drawCircle(vx + valW + 5, vy + 4, 2, fg);   // ° symbol
      spr.drawString("C", vx + valW + 10, vy, 2);
    } else {
      spr.drawString("--", x + 52, 50 + 2, 2);
    }
  }

  // Row 2 (y=72) — Engine Load
  {
    bool ok = d.engineLoadValid;
    snprintf(buf, sizeof(buf), ok ? "%u %%" : "--", d.engineLoad);
    drawRow(72, "Load", buf, ok,
            ok ? loadStatus(d.engineLoad) : 2,
            ok ? d.engineLoad : 0, true);
  }

  // Row 3 (y=94) — Battery Voltage
  {
    bool ok = d.batteryVoltageValid;
    snprintf(buf, sizeof(buf), ok ? "%.1fV" : "--", d.batteryVoltage);
    drawRow(94, "Batt", buf, ok,
            ok ? battStatus(d.batteryVoltage) : 2,
            ok ? battPct(d.batteryVoltage) : 0, true);
  }

  // Row 4 (y=116) — Throttle Position
  {
    bool ok = d.throttlePosValid;
    snprintf(buf, sizeof(buf), ok ? "%u %%" : "--", d.throttlePos);
    drawRow(116, "Throt", buf, ok,
            ok ? throttleStatus(d.throttlePos) : 2,
            ok ? d.throttlePos : 0, true);
  }

  // Row 5 (y=138) — Intake Temp (no mini bar)
  {
    bool ok = d.intakeTempValid;
    fg = ok ? TH.text : GRAY;
    drawRow(138, "Intake", "", ok,
            ok ? intakeStatus(d.intakeTemp) : 2,
            ok ? intakePct(d.intakeTemp) : 0, false);
    spr.setTextColor(fg);
    if (ok) {
      char val[8];
      snprintf(val, sizeof(val), "%d", d.intakeTemp);
      int vx = x + 52, vy = 138 + 2;
      spr.drawString(val, vx, vy, 2);
      int valW = spr.textWidth(val, 2);
      spr.drawCircle(vx + valW + 5, vy + 4, 2, fg);
      spr.drawString("C", vx + valW + 10, vy, 2);
    } else {
      spr.drawString("--", x + 52, 138 + 2, 2);
    }
  }
}

// ─────────────────────────────────────────────────────────
// Tachometer arc gauge (drawSmoothArc + fillTriangle needle)
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
  // Tick marks at every 1000 RPM
  //
  for (unsigned int r = 0; r <= 7000; r += 1000)
  {
    int  angle = 135 + (int)(r * 270u / OBD_MAX_RPM);
    float rad  = angle * (PI / 180.0f);
    float c    = cosf(rad);
    float s    = sinf(rad);
    int x1 = cx + (int)((outerR - 8) * c);
    int y1 = cy + (int)((outerR - 8) * s);
    int x2 = cx + (int)((outerR - 1) * c);
    int y2 = cy + (int)((outerR - 1) * s);
    spr.drawLine(x1, y1, x2, y2, TH.text);
  }

  //
  // SHIFT progression bars (top of gauge)
  //
  int numBars = 0;
  if (rpm > 5000) numBars = 1;
  if (rpm > 5300) numBars = 2;
  if (rpm > 5600) numBars = 3;
  if (rpm > 5900) numBars = 4;

  const int barW = 8, barH = 5, barGap = 3;
  int totalW     = 4 * barW + 3 * barGap;
  int barY       = cy - outerR - 7;
  int barStartX  = cx - totalW / 2;

  for (int i = 0; i < 4; i++)
  {
    int bx       = barStartX + i * (barW + barGap);
    uint16_t col = (i < numBars) ? TH.text_warn : TH.smeter_bar_empty;
    spr.fillRect(bx, barY, barW, barH, col);
  }

  //
  // Needle (fillTriangle)
  //
  float ndlAngle = 135.0f + (rpm * 270.0f) / OBD_MAX_RPM;
  float rad_     = ndlAngle * (PI / 180.0f);
  float ndlC     = cosf(rad_);
  float ndlS     = sinf(rad_);

  // Tip near outer arc edge
  int tipX = cx + (int)((outerR - 3) * ndlC);
  int tipY = cy + (int)((outerR - 3) * ndlS);

  // Base: extends from center along needle direction with width
  int bCx = cx + (int)(10 * ndlC);
  int bCy = cy + (int)(10 * ndlS);

  float perpC = cosf(rad_ + PI / 2.0f);
  float perpS = sinf(rad_ + PI / 2.0f);
  int bHalfW  = 5;

  spr.fillTriangle(tipX, tipY,
                   bCx + (int)(bHalfW * perpC), bCy + (int)(bHalfW * perpS),
                   bCx - (int)(bHalfW * perpC), bCy - (int)(bHalfW * perpS),
                   TH.freq_text);
  // Pivot dot
  spr.fillCircle(cx, cy, 4, TH.freq_text);

  //
  // SHIFT! blinking overlay
  //
  if (rpm >= SHIFT_RPM_LIMIT && ((millis() / 500) & 1))
  {
    spr.setFreeFont(&Orbitron_Light_24);
    spr.setTextColor(TH.text_warn);
    spr.setTextDatum(TC_DATUM);
    spr.drawString("SHIFT!", cx, cy + 42, 4);
    spr.setFreeFont(NULL);
  }

  //
  // Center RPM number (drawn last so it's always on top of needle)
  //
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextColor(TH.freq_text, TH.bg);
  spr.setTextDatum(TC_DATUM);

  char rpmStr[8];
  snprintf(rpmStr, sizeof(rpmStr), "%u", rpm);

  // Wide padding ensures needle base does not show through character gaps
  spr.setTextPadding(spr.textWidth("8888", 4) + 6);
  spr.drawString(rpmStr, cx, cy - 8, 4);
  spr.setTextPadding(0);

  spr.setFreeFont(NULL);
  spr.setTextColor(TH.text_muted);
  spr.setTextDatum(TC_DATUM);
  spr.drawString("RPM", cx, cy + 20, 2);
}

// ─────────────────────────────────────────────────────────
// Main OBD layout entry point
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

  // ── Gauge + panel ─────────────────────────────────────
  drawObdGauge(82, 95, 65, 50, d.rpm);
  drawObdPanel(170, 22, 145, d);

}
