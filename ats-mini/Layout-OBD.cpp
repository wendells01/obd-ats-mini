#include "Common.h"
#include "Themes.h"
#include "Menu.h"
#include "Draw.h"
#include "BleMode.h"

// ─────────────────────────────────────────────────────────
// Draw a labelled progress bar
// ─────────────────────────────────────────────────────────
static void drawObdBar(int x, int y, int w, int h, const char* label,
                        uint8_t percent, const char* value)
{
  // Label
  spr.setTextColor(TH.text);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(label, x, y, 2);

  // Bar background
  int barX  = x + 65;
  int barY  = y + 2;
  int barW  = w - 65 - 50;  // remaining width for bar
  int barH  = h - 4;

  if (barW > 4)
  {
    spr.drawRect(barX, barY, barW, barH, TH.smeter_bar_empty);

    uint16_t fillColor = TH.smeter_bar;
    if (percent > 85)
      fillColor = TH.text_warn;  // red for near‑limit
    else if (percent > 60)
      fillColor = TH.smeter_bar_plus;  // yellow for mid‑high

    int fillW = (int)((uint16_t)barW * percent / 100);
    if (fillW > 0)
      spr.fillRect(barX + 1, barY + 1, fillW, barH - 2, fillColor);
  }

  // Value text
  spr.setTextColor(TH.text);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(value, x + w - 46, y, 2);
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

  spr.setTextColor(TH.text_muted);
  spr.drawString("RPM", 280, 2, 2);

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
    spr.drawString("Click to exit", 5, 150, 2);
    return;
  }

  // ── RPM (large, centred) ──────────────────────────────
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextColor(TH.freq_text);
  spr.setTextDatum(TC_DATUM);

  char rpmStr[12];
  snprintf(rpmStr, sizeof(rpmStr), "%u", d.rpm);
  spr.drawString(rpmStr, 160, 24, 4);

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.text_muted);
  spr.drawString("RPM", 160, 58, 2);

  // ── Data rows ─────────────────────────────────────────
  // Two columns:        column 1            column 2
  // Row 0:     Speed           (y=75)     Coolant Temp      (y=75)
  // Row 1:     Engine Load     (y=100)    Throttle Position (y=100)
  // Row 2:     Intake Temp     (y=125)    Battery Voltage   (y=125)

  int colW = 155;  // width per column

  // Column 1
  {
    char buf[16];

    // Speed
    snprintf(buf, sizeof(buf), "%u km/h", d.speed);
    drawObdBar(5, 75, colW, 20, "Speed",
               d.speedValid ? (d.speed * 100 / 200) : 0,
               d.speedValid ? buf : "--");

    // Engine Load
    snprintf(buf, sizeof(buf), "%u %%", d.engineLoad);
    drawObdBar(5, 100, colW, 20, "Load",
               d.engineLoadValid ? d.engineLoad : 0,
               d.engineLoadValid ? buf : "--");

    // Intake Temp
    snprintf(buf, sizeof(buf), "%d C", d.intakeTemp);
    drawObdBar(5, 125, colW, 20, "Intake",
               d.intakeTempValid ? (uint8_t)((d.intakeTemp + 40) * 100 / 80) : 0,
               d.intakeTempValid ? buf : "--");
  }

  // Column 2
  {
    char buf[16];

    // Coolant Temp
    snprintf(buf, sizeof(buf), "%d C", d.coolantTemp);
    uint8_t pct = d.coolantTempValid
                    ? (uint8_t)((d.coolantTemp + 40) * 100 / 160) : 0;
    drawObdBar(165, 75, colW, 20, "Coolant", pct,
               d.coolantTempValid ? buf : "--");

    // Throttle Position
    snprintf(buf, sizeof(buf), "%u %%", d.throttlePos);
    drawObdBar(165, 100, colW, 20, "Throttle",
               d.throttlePosValid ? d.throttlePos : 0,
               d.throttlePosValid ? buf : "--");

    // Battery Voltage
    snprintf(buf, sizeof(buf), "%.1f V", (double)d.batteryVoltage);
    uint8_t battPct = d.batteryVoltageValid
                        ? (uint8_t)((d.batteryVoltage - 10.0f) * 100 / 6.0f)
                        : 0;
    drawObdBar(165, 125, colW, 20, "Battery", battPct,
               d.batteryVoltageValid ? buf : "--");
  }

  // ── Temperature alert overlay (coolant > 100°C) ─────
  {
    static bool alertActive = false;

    // Hysteresis: activate at >100°C, clear at <95°C
    if (d.coolantTempValid && d.coolantTemp > 100)
      alertActive = true;
    else if (d.coolantTemp < 95)
      alertActive = false;

    // Blink overlay at 500ms interval
    if (alertActive && ((millis() / 500) & 1))
    {
      spr.fillRect(50, 55, 220, 60, TH.text_warn);

      spr.setFreeFont(&Orbitron_Light_24);
      spr.setTextColor(TFT_WHITE);
      spr.setTextDatum(TC_DATUM);
      spr.drawString("TEMP!", 160, 78, 4);

      spr.setFreeFont(NULL);
      char tmp[16];
      snprintf(tmp, sizeof(tmp), "%d C", d.coolantTemp);
      spr.drawString(tmp, 160, 105, 2);
    }
  }

  // ── Help text ─────────────────────────────────────────
  spr.setTextColor(TH.text_muted);
  spr.setTextDatum(TL_DATUM);
  spr.drawString("Click to exit", 5, 150, 2);
}
