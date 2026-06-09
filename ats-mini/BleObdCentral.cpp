#include "BleObdCentral.h"
#include <string.h>
#include <ctype.h>
#include <math.h>

static BLEUUID obdServiceUUID((uint16_t)OBD_SERVICE_UUID);
static BLEUUID txCharUUID((uint16_t)OBD_TX_CHAR_UUID);
static BLEUUID rxCharUUID((uint16_t)OBD_RX_CHAR_UUID);
static BLEUUID altObdServiceUUID((uint16_t)0xFFF1);

// Scan parameters — OBD adapters may advertise infrequently
#define OBD_SCAN_INTERVAL  200
#define OBD_SCAN_WINDOW    150

BleObdCentral* BleObdCentral::activeInstance_ = nullptr;

// ------------------------------------------------------------------
// Helpers : hex nibble / token parser
// ------------------------------------------------------------------

static uint8_t hexNibble(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

// Tokenise hex bytes from a free‑form ELM327 response string.
// Returns the number of bytes decoded (max maxValues).
static uint8_t parseHexTokens(const char* resp, uint8_t* values, uint8_t maxValues)
{
  uint8_t count = 0;
  const char* p = resp;

  while (*p && count < maxValues)
  {
    // Skip non‑hex characters
    while (*p && !isxdigit((unsigned char)*p)) p++;
    if (!*p) break;

    // Read one hex byte (may be followed by another hex digit → two digits)
    uint8_t byte = hexNibble(*p);
    p++;
    if (isxdigit((unsigned char)*p))
    {
      byte = (byte << 4) | hexNibble(*p);
      p++;
    }
    values[count++] = byte;
  }

  return count;
}

// ------------------------------------------------------------------
// BleObdCentral implementation
// ------------------------------------------------------------------

void BleObdCentral::configureSecurity()
{
  security_.setCapability(ESP_IO_CAP_NONE);
  security_.setAuthenticationMode(true, false, true);
  BLESecurity::setForceAuthentication(false);
  BLEDevice::setSecurityCallbacks(&securityCallbacks_);
}

void BleObdCentral::configureScan(BLEScan& scan)
{
  scan.setInterval(OBD_SCAN_INTERVAL);
  scan.setWindow(OBD_SCAN_WINDOW);
  scan.setActiveScan(true);
}

void BleObdCentral::configureClient()
{
  BLEClient* currentClient = client();
  if (currentClient == nullptr) return;
  currentClient->setMTU(185);
}

bool BleObdCentral::acceptsAdvertisement(BLEAdvertisedDevice& device)
{
  return device.isConnectable() &&
         device.haveServiceUUID() &&
         (device.isAdvertisingService(obdServiceUUID) ||
          device.isAdvertisingService(altObdServiceUUID));
}

bool BleObdCentral::setupConnectedPeer()
{
  BLEClient* currentClient = client();
  if (currentClient == nullptr) return false;

  BLERemoteService* obdService = currentClient->getService(obdServiceUUID);
  if (obdService == nullptr) return false;

  // Discover TX (notify) characteristic
  BLERemoteCharacteristic* txChar = obdService->getCharacteristic(txCharUUID);
  if (txChar == nullptr || !txChar->canNotify()) return false;

  // Discover RX (write) characteristic
  BLERemoteCharacteristic* rxChar = obdService->getCharacteristic(rxCharUUID);
  if (rxChar == nullptr || !rxChar->canWrite()) return false;

  // Subscribe to notifications
  activeInstance_ = this;
  txChar->registerForNotify(notifyCallback, true);

  txChar_ = txChar;
  rxChar_ = rxChar;
  obdState_ = ObdConnectionState::Connected;

  // Start ELM327 initialisation sequence
  clearResponse();
  elmState_ = ElmState::InitATZ;
  initStep_ = 0;
  initRetries_ = 0;
  lastCmdMs_ = millis();
  cmdStartMs_ = 0;

  return true;
}

void BleObdCentral::resetConnectedPeerState()
{
  txChar_ = nullptr;
  rxChar_ = nullptr;
  obdState_ = ObdConnectionState::Disconnected;
  elmState_ = ElmState::InitATZ;
  pidCount_ = 0;
  currentPid_ = 0;
  abortPending_ = false;
  cmdStartMs_ = 0;
  clearResponse();
  if (activeInstance_ == this)
    activeInstance_ = nullptr;
}

// ------------------------------------------------------------------
// Notify callback (called from NimBLE context)
// ------------------------------------------------------------------

void BleObdCentral::notifyCallback(
  BLERemoteCharacteristic* characteristic,
  uint8_t* data,
  size_t length,
  bool isNotify)
{
  (void)isNotify;
  if (activeInstance_ == nullptr || characteristic == nullptr || data == nullptr || length == 0)
    return;

  if (characteristic->getUUID().equals(txCharUUID))
    activeInstance_->accumulateResponse((const char*)data, length);
}

// ------------------------------------------------------------------
// Response accumulation
// ------------------------------------------------------------------

void BleObdCentral::accumulateResponse(const char* data, size_t length)
{
  if (data == nullptr || length == 0) return;

  size_t space = OBD_MAX_RESPONSE - responseLength_ - 1;
  if (space > 0)
  {
    size_t toCopy = (length < space) ? length : space;
    memcpy(responseBuffer_ + responseLength_, data, toCopy);
    responseLength_ += toCopy;
    responseBuffer_[responseLength_] = '\0';
  }

  // Mark ready when we see the ELM327 prompt '>'
  if (responseLength_ > 0)
  {
    char last = responseBuffer_[responseLength_ - 1];
    if (last == '>')
      responseReady_ = true;
  }
}

void BleObdCentral::clearResponse()
{
  responseLength_ = 0;
  responseBuffer_[0] = '\0';
  responseReady_ = false;
}

bool BleObdCentral::isResponseReady() const
{
  return responseReady_;
}

const char* BleObdCentral::readResponse()
{
  return responseBuffer_;
}

// ------------------------------------------------------------------
// Command I/O
// ------------------------------------------------------------------

void BleObdCentral::writeCommand(const char* cmd)
{
  if (rxChar_ == nullptr || cmd == nullptr) return;

  size_t len = strlen(cmd);
  rxChar_->writeValue((uint8_t*)cmd, len, false);
  clearResponse();
}

void BleObdCentral::sendNextCommand(const char* cmd)
{
  writeCommand(cmd);
  cmdStartMs_ = millis();
  lastCmdMs_ = cmdStartMs_;
}

// ------------------------------------------------------------------
// ELM327 initialisation
// ------------------------------------------------------------------

void BleObdCentral::advanceInit()
{
  switch (elmState_)
  {
    case ElmState::InitATZ:
      elmState_ = ElmState::InitATE0;
      break;
    case ElmState::InitATE0:
      elmState_ = ElmState::InitATL0;
      break;
    case ElmState::InitATL0:
      elmState_ = ElmState::InitATS0;
      break;
    case ElmState::InitATS0:
      elmState_ = ElmState::InitATH0;
      break;
    case ElmState::InitATH0:
      elmState_ = ElmState::InitATSP0;
      break;
    case ElmState::InitATSP0:
      elmState_ = ElmState::Ready;
      buildPidQueue();
      break;
    default:
      break;
  }
  initRetries_ = 0;
  clearResponse();
}

// ------------------------------------------------------------------
// PID queue
// ------------------------------------------------------------------

void BleObdCentral::buildPidQueue()
{
  pidCount_ = 0;

  // Fast (250 ms)
  addPidEntry(0x0C, 250, parseRpm);
  // Medium (500 ms)
  addPidEntry(0x0D, 500, parseSpeed);
  // Slow (1 s)
  addPidEntry(0x05, 1000, parseCoolantTemp);
  addPidEntry(0x04, 1000, parseEngineLoad);
  addPidEntry(0x0F, 1000, parseIntakeTemp);
  addPidEntry(0x10, 1000, parseMafRate);
  addPidEntry(0x11, 1000, parseThrottlePos);
  // Very slow (5 s)
  addPidEntry(0x0E, 5000, parseTimingAdvance);
  addPidEntry(0x2F, 5000, parseFuelLevel);
  addPidEntry(0x42, 5000, parseBatteryVoltage);

  currentPid_ = 0;
}

void BleObdCentral::addPidEntry(uint8_t pid, uint32_t intervalMs,
                                 bool (*parser)(const char*, ObdData&))
{
  if (pidCount_ >= (uint8_t)(sizeof(pidQueue_) / sizeof(pidQueue_[0]))) return;
  pidQueue_[pidCount_].pid          = pid;
  pidQueue_[pidCount_].intervalMs   = intervalMs;
  pidQueue_[pidCount_].lastPolledMs = 0;
  pidQueue_[pidCount_].parser       = parser;
  pidCount_++;
}

void BleObdCentral::pollNextPid()
{
  if (pidCount_ == 0) return;

  uint32_t now = millis();
  uint8_t start = currentPid_;

  for (uint8_t i = 0; i < pidCount_; i++)
  {
    uint8_t idx = (start + i) % pidCount_;
    if (now - pidQueue_[idx].lastPolledMs >= pidQueue_[idx].intervalMs)
    {
      currentPid_ = (idx + 1) % pidCount_;
      pidQueue_[idx].lastPolledMs = now;

      char cmd[8];
      snprintf(cmd, sizeof(cmd), "%02X%02X\r", 1, pidQueue_[idx].pid);
      sendNextCommand(cmd);
      return;
    }
  }
}

// ------------------------------------------------------------------
// Response processing (dispatched from update when data arrives)
// ------------------------------------------------------------------

void BleObdCentral::processResponse()
{
  // Trim trailing '>', \r, \n, spaces
  char* resp = responseBuffer_;
  size_t len = responseLength_;

  while (len > 0)
  {
    char c = resp[len - 1];
    if (c == '>' || c == '\r' || c == '\n' || c == ' ' || c == '\t')
      resp[--len] = '\0';
    else
      break;
  }

  if (elmState_ < ElmState::Ready)
  {
    // --- ELM327 init step ------------------------------------------------
    // ATZ accepts any non‑empty response; all other steps expect "OK"
    bool ok = (len > 0);
    if (elmState_ != ElmState::InitATZ)
      ok = (strstr(resp, "OK") != nullptr);

    if (ok)
    {
      advanceInit();
    }
    else
    {
      initRetries_++;
      if (initRetries_ >= OBD_INIT_RETRIES)
      {
        elmState_ = ElmState::Error;
        clearResponse();
        return;
      }
    }
    // The next update() tick will re‑send the same init command
  }
  else if (elmState_ == ElmState::Ready && pidCount_ > 0)
  {
    // --- PID response ----------------------------------------------------
    uint8_t prevPid = (currentPid_ == 0)
                        ? pidCount_ - 1
                        : currentPid_ - 1;
    if (pidQueue_[prevPid].parser)
    {
      pidQueue_[prevPid].parser(resp, obdData_);
      obdData_.updated = millis();
    }
  }

  clearResponse();
}

// ------------------------------------------------------------------
// Main tick – called from bleLoop every ~5 ms
// ------------------------------------------------------------------

void BleObdCentral::update()
{
  // Auto-reconnect on disconnect
  if (!isConnected() && isStarted())
  {
    if (cmdStartMs_ == 0 || cmdStartMs_ == 1)
    {
      clearResponse();
      beginScan();
      cmdStartMs_ = 1;
    }
    return;
  }

  // Invalidate stale data after 30s of no response
  if (elmState_ == ElmState::Ready && obdData_.updated > 0 && (millis() - obdData_.updated > 30000))
  {
    obdData_.rpmValid = false;
    obdData_.speedValid = false;
    obdData_.coolantTempValid = false;
    obdData_.engineLoadValid = false;
    obdData_.intakeTempValid = false;
    obdData_.mafRateValid = false;
    obdData_.throttlePosValid = false;
    obdData_.timingAdvanceValid = false;
    obdData_.fuelLevelValid = false;
    obdData_.batteryVoltageValid = false;
  }

  uint32_t now = millis();

  // Detect stalled command (no response within OBD_CMD_TIMEOUT)
  if (cmdStartMs_ != 0 && (now - cmdStartMs_ > OBD_CMD_TIMEOUT))
  {
    clearResponse();
    cmdStartMs_ = 0;
    lastCmdMs_ = now;

    // On timeout during init, treat as failure (retry will happen naturally)
    if (elmState_ < ElmState::Ready && elmState_ != ElmState::Error)
    {
      initRetries_++;
      if (initRetries_ >= OBD_INIT_RETRIES)
        elmState_ = ElmState::Error;
    }
  }

  // Process incoming response
  if (isResponseReady())
  {
    processResponse();
    cmdStartMs_ = 0;
    lastCmdMs_ = millis();
  }

  // Send next command if none pending and minimum interval has elapsed
  if (cmdStartMs_ == 0 && (millis() - lastCmdMs_ >= OBD_PID_INTERVAL))
  {
    if (elmState_ < ElmState::Ready && elmState_ != ElmState::Error)
    {
      sendNextCommand(initCommands_[(uint8_t)elmState_]);
    }
    else if (elmState_ == ElmState::Ready)
    {
      pollNextPid();
    }
  }

  // Demo mode: driving simulation state machine
  if(demoMode_ && !isConnected())
  {
    uint32_t now = millis();
    if(now - lastDemoUpdateMs_ > 100)
    {
      lastDemoUpdateMs_ = now;

      // Interpolation helper: t in [0, 1]
      auto lerp = [](float a, float b, float t) -> float { return a + (b - a) * t; };
      auto lerpu8 = [&](uint8_t a, uint8_t b, float t) -> uint8_t { return (uint8_t)(lerp(a, b, t) + 0.5f); };
      auto lerps8 = [&](int8_t a, int8_t b, float t) -> int8_t { return (int8_t)(lerp(a, b, t) + 0.5f); };
      auto lerpu16 = [&](uint16_t a, uint16_t b, float t) -> uint16_t { return (uint16_t)(lerp(a, b, t) + 0.5f); };

      uint32_t elapsed = now - demoPhaseStartMs_;
      float t;
      bool advance = true;

      switch(demoPhase_)
      {
        case DemoPhase::Idle:
          // 3s: engine idling
          t = fmin((float)elapsed / 3000.0f, 1.0f);
          obdData_.rpm        = 800;
          obdData_.speed      = 0;
          obdData_.coolantTemp= lerps8(87, 88, t);
          obdData_.engineLoad = lerpu8(15, 20, t);
          obdData_.throttlePos= lerpu8(0, 5, t);
          break;

        case DemoPhase::Accelerating:
          // 8s: full acceleration 900→6500 RPM, 0→120 km/h
          t = fmin((float)elapsed / 8000.0f, 1.0f);
          obdData_.rpm        = lerpu16(900, 6500, t);
          obdData_.speed      = lerpu8(0, 120, t);
          obdData_.coolantTemp= lerps8(88, 95, t);
          obdData_.engineLoad = lerpu8(20, 80, t);
          obdData_.throttlePos= lerpu8(5, 60, t);
          break;

        case DemoPhase::ShiftCoast:
          // 1.5s: clutch in — RPM drops 6500→4000, speed coasts up
          t = fmin((float)elapsed / 1500.0f, 1.0f);
          obdData_.rpm        = lerpu16(6500, 4000, t);
          obdData_.speed      = lerpu8(120, 125, t);
          obdData_.coolantTemp= lerps8(95, 96, t);
          obdData_.engineLoad = lerpu8(80, 40, t);
          obdData_.throttlePos= lerpu8(60, 10, t);
          break;

        case DemoPhase::Accelerating2:
          // 4s: 2nd gear pull 4000→6500 RPM, 125→160 km/h
          t = fmin((float)elapsed / 4000.0f, 1.0f);
          obdData_.rpm        = lerpu16(4000, 6500, t);
          obdData_.speed      = lerpu8(125, 160, t);
          obdData_.coolantTemp= lerps8(96, 102, t);
          obdData_.engineLoad = lerpu8(40, 85, t);
          obdData_.throttlePos= lerpu8(10, 65, t);
          break;

        case DemoPhase::ShiftCoast2:
          // 1.5s: 2nd shift — RPM 6500→4000, speed 160→165
          t = fmin((float)elapsed / 1500.0f, 1.0f);
          obdData_.rpm        = lerpu16(6500, 4000, t);
          obdData_.speed      = lerpu8(160, 165, t);
          obdData_.coolantTemp= lerps8(102, 103, t);
          obdData_.engineLoad = lerpu8(85, 45, t);
          obdData_.throttlePos= lerpu8(65, 12, t);
          break;

        case DemoPhase::Cruising:
          // 6s: cruising — RPM tapers 4000→2200, speed coasts down
          t = fmin((float)elapsed / 6000.0f, 1.0f);
          obdData_.rpm        = lerpu16(4000, 2200, t);
          obdData_.speed      = lerpu8(165, 100, t);
          obdData_.coolantTemp= lerps8(103, 97, t);
          obdData_.engineLoad = lerpu8(45, 30, t);
          obdData_.throttlePos= lerpu8(12, 18, t);
          break;

        case DemoPhase::Decelerating:
          // 5s: braking — RPM 2200→800, speed 100→5
          t = fmin((float)elapsed / 5000.0f, 1.0f);
          obdData_.rpm        = lerpu16(2200, 800, t);
          obdData_.speed      = lerpu8(100, 5, t);
          obdData_.coolantTemp= lerps8(97, 92, t);
          obdData_.engineLoad = lerpu8(30, 10, t);
          obdData_.throttlePos= lerpu8(18, 2, t);
          break;

        case DemoPhase::Stopping:
          // 2s: final stop — RPM 800→0, speed 5→0
          t = fmin((float)elapsed / 2000.0f, 1.0f);
          obdData_.rpm        = lerpu16(800, 0, t);
          obdData_.speed      = lerpu8(5, 0, t);
          obdData_.coolantTemp= lerps8(92, 90, t);
          obdData_.engineLoad = lerpu8(10, 5, t);
          obdData_.throttlePos= lerpu8(2, 0, t);
          break;

        default:
          advance = false;
          break;
      }

      // Always set these during demo
      obdData_.rpmValid         = true;
      obdData_.speedValid       = true;
      obdData_.coolantTempValid = true;
      obdData_.engineLoadValid  = true;
      obdData_.throttlePosValid = true;
      obdData_.intakeTemp       = 40;
      obdData_.intakeTempValid  = true;
      obdData_.batteryVoltage   = 12.6f;
      obdData_.batteryVoltageValid = true;
      obdData_.fuelLevel        = 75;
      obdData_.fuelLevelValid   = true;
      obdData_.timingAdvance    = (int8_t)(lerp(8, 15, fmin(obdData_.rpm / 6500.0f, 1.0f)) + 0.5f);
      obdData_.timingAdvanceValid = true;
      obdData_.mafRate          = (uint16_t)(lerp(200, 600, fmin(obdData_.rpm / 6500.0f, 1.0f)) + 0.5f);
      obdData_.mafRateValid     = true;

      obdData_.updated = now;

      // Advance to next phase when current one completes
      if(advance && t >= 1.0f)
      {
        demoPhase_ = static_cast<DemoPhase>((uint8_t)demoPhase_ + 1);
        if(demoPhase_ > DemoPhase::Stopping)
          demoPhase_ = DemoPhase::Idle;
        demoPhaseStartMs_ = now;
      }
    }
  }
}

// ------------------------------------------------------------------
// Public helpers
// ------------------------------------------------------------------

ObdConnectionState BleObdCentral::obdConnectionState() const
{
  return obdState_;
}

bool BleObdCentral::consumeAbortPending()
{
  bool pending = abortPending_;
  abortPending_ = false;
  return pending;
}

void BleObdCentral::enableDemoMode(bool enable)
{
  demoMode_ = enable;
  if(enable)
  {
    demoPhase_ = DemoPhase::Idle;
    demoPhaseStartMs_ = millis();
    lastDemoUpdateMs_ = millis();

    obdData_.rpm = 800;
    obdData_.speed = 0;
    obdData_.coolantTemp = 87;
    obdData_.engineLoad = 15;
    obdData_.intakeTemp = 40;
    obdData_.throttlePos = 0;
    obdData_.batteryVoltage = 12.6f;
    obdData_.fuelLevel = 75;
    obdData_.timingAdvance = 8;
    obdData_.mafRate = 200;

    obdData_.rpmValid = true;
    obdData_.speedValid = true;
    obdData_.coolantTempValid = true;
    obdData_.engineLoadValid = true;
    obdData_.intakeTempValid = true;
    obdData_.throttlePosValid = true;
    obdData_.batteryVoltageValid = true;
    obdData_.fuelLevelValid = true;
    obdData_.timingAdvanceValid = true;
    obdData_.mafRateValid = true;
    obdData_.updated = millis();
  }
}

bool BleObdCentral::isDemoMode() const
{
  return demoMode_;
}

// ------------------------------------------------------------------
// PID parsers
// ------------------------------------------------------------------

bool BleObdCentral::parseRpm(const char* resp, ObdData& data)
{
  // Expected hex tokens: [.. 41 0C <hi> <lo> ..]
  uint8_t v[8];
  uint8_t n = parseHexTokens(resp, v, 8);
  for (uint8_t i = 0; i + 3 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x0C)
    {
      data.rpm = ((uint16_t)v[i + 2] << 8 | v[i + 3]) / 4;
      data.rpmValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseSpeed(const char* resp, ObdData& data)
{
  uint8_t v[4];
  uint8_t n = parseHexTokens(resp, v, 4);
  for (uint8_t i = 0; i + 2 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x0D)
    {
      data.speed = v[i + 2];
      data.speedValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseCoolantTemp(const char* resp, ObdData& data)
{
  uint8_t v[4];
  uint8_t n = parseHexTokens(resp, v, 4);
  for (uint8_t i = 0; i + 2 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x05)
    {
      data.coolantTemp = (int8_t)(v[i + 2]) - 40;
      data.coolantTempValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseEngineLoad(const char* resp, ObdData& data)
{
  uint8_t v[4];
  uint8_t n = parseHexTokens(resp, v, 4);
  for (uint8_t i = 0; i + 2 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x04)
    {
      data.engineLoad = (uint8_t)((uint16_t)v[i + 2] * 100 / 255);
      data.engineLoadValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseIntakeTemp(const char* resp, ObdData& data)
{
  uint8_t v[4];
  uint8_t n = parseHexTokens(resp, v, 4);
  for (uint8_t i = 0; i + 2 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x0F)
    {
      data.intakeTemp = (int8_t)(v[i + 2]) - 40;
      data.intakeTempValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseMafRate(const char* resp, ObdData& data)
{
  uint8_t v[8];
  uint8_t n = parseHexTokens(resp, v, 8);
  for (uint8_t i = 0; i + 3 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x10)
    {
      data.mafRate = (uint16_t)v[i + 2] << 8 | v[i + 3];
      data.mafRateValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseThrottlePos(const char* resp, ObdData& data)
{
  uint8_t v[4];
  uint8_t n = parseHexTokens(resp, v, 4);
  for (uint8_t i = 0; i + 2 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x11)
    {
      data.throttlePos = (uint8_t)((uint16_t)v[i + 2] * 100 / 255);
      data.throttlePosValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseTimingAdvance(const char* resp, ObdData& data)
{
  uint8_t v[4];
  uint8_t n = parseHexTokens(resp, v, 4);
  for (uint8_t i = 0; i + 2 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x0E)
    {
      data.timingAdvance = (int8_t)((int)v[i + 2] / 2 - 64);
      data.timingAdvanceValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseFuelLevel(const char* resp, ObdData& data)
{
  uint8_t v[4];
  uint8_t n = parseHexTokens(resp, v, 4);
  for (uint8_t i = 0; i + 2 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x2F)
    {
      data.fuelLevel = (uint8_t)((uint16_t)v[i + 2] * 100 / 255);
      data.fuelLevelValid = true;
      return true;
    }
  }
  return false;
}

bool BleObdCentral::parseBatteryVoltage(const char* resp, ObdData& data)
{
  uint8_t v[4];
  uint8_t n = parseHexTokens(resp, v, 4);
  for (uint8_t i = 0; i + 2 < n; i++)
  {
    if (v[i] == 0x41 && v[i + 1] == 0x42)
    {
      data.batteryVoltage = (float)v[i + 2] * 0.1f;
      data.batteryVoltageValid = true;
      return true;
    }
  }
  return false;
}
