#ifndef BLE_OBD_CENTRAL_H
#define BLE_OBD_CENTRAL_H

#include "BleCentral.h"

// Standard OBD-II BLE service UUID (ELM327/vLinker adapters)
#define OBD_SERVICE_UUID     0xFFF0
#define OBD_TX_CHAR_UUID     0xFFF1  // Peripheral -> Central (notify)
#define OBD_RX_CHAR_UUID     0xFFF2  // Central -> Peripheral (write)

// Response buffer
#define OBD_MAX_RESPONSE     160

// ELM327 command timeout and PID intervals
#define OBD_CMD_TIMEOUT      2000   // 2s timeout per command
#define OBD_PID_INTERVAL     100    // 100ms minimum between commands
#define OBD_INIT_RETRIES      3     // Max retries per init step

// OBD connection state (for LCD status indicator)
enum class ObdConnectionState : uint8_t {
  Disconnected = 0,
  Scanning,
  Connecting,
  Connected,
};

// Parsed OBD data with validity flags
struct ObdData {
  uint32_t updated = 0;       // millis() of last successful poll
  uint16_t rpm = 0;           // 0x0C, value / 4 = RPM
  uint8_t speed = 0;          // 0x0D, km/h
  int8_t coolantTemp = 0;    // 0x05, °C
  uint8_t engineLoad = 0;    // 0x04, %
  int8_t intakeTemp = 0;     // 0x0F, °C
  uint16_t mafRate = 0;      // 0x10, g/s * 100
  uint8_t throttlePos = 0;   // 0x11, %
  int8_t timingAdvance = 0;  // 0x0E, degrees
  uint8_t fuelLevel = 0;     // 0x2F, %
  float batteryVoltage = 0;  // 0x42, V

  bool rpmValid = false;
  bool speedValid = false;
  bool coolantTempValid = false;
  bool engineLoadValid = false;
  bool intakeTempValid = false;
  bool mafRateValid = false;
  bool throttlePosValid = false;
  bool timingAdvanceValid = false;
  bool fuelLevelValid = false;
  bool batteryVoltageValid = false;
};

class BleObdCentral : public BleCentral {
public:
  BleObdCentral() = default;

  void writeCommand(const char* cmd);
  const char* readResponse();
  bool isResponseReady() const;
  void clearResponse();
  void update();

  ObdConnectionState obdConnectionState() const;
  bool consumeAbortPending();

  const ObdData& obdData() const { return obdData_; }
  bool isReady() const { return demoMode_ || elmState_ == ElmState::Ready; }

  void enableDemoMode(bool enable);
  bool isDemoMode() const;

protected:
  void configureSecurity() override;
  void configureScan(BLEScan& scan) override;
  void configureClient() override;
  bool acceptsAdvertisement(BLEAdvertisedDevice& device) override;
  bool setupConnectedPeer() override;
  void resetConnectedPeerState() override;

private:
  // ELM327 initialization state machine
  enum class ElmState : uint8_t {
    InitATZ,     // Reset
    InitATE0,    // Echo off
    InitATL0,    // Linefeeds off
    InitATS0,    // Spaces off
    InitATH0,    // Headers off
    InitATSP0,   // Auto protocol
    Ready,       // Initialized, polling PIDs
    Error,       // Init failed after retries
  };

  struct PidEntry {
    uint8_t pid;
    uint32_t intervalMs;
    uint32_t lastPolledMs;
    bool (*parser)(const char* response, ObdData& data);
  };

  class SecurityCallbacks : public BLESecurityCallbacks {
    void onAuthenticationComplete(ble_gap_conn_desc *desc) override { (void)desc; }
  };

  static void notifyCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify);

  void accumulateResponse(const char* data, size_t length);
  void processResponse();
  void runDemoSimulation();

  void advanceInit();
  void sendNextCommand(const char* cmd);

  void pollNextPid();
  void buildPidQueue();
  void addPidEntry(uint8_t pid, uint32_t intervalMs, bool (*parser)(const char*, ObdData&));

  // PID response parsers
  static bool parseRpm(const char* resp, ObdData& data);
  static bool parseSpeed(const char* resp, ObdData& data);
  static bool parseCoolantTemp(const char* resp, ObdData& data);
  static bool parseEngineLoad(const char* resp, ObdData& data);
  static bool parseIntakeTemp(const char* resp, ObdData& data);
  static bool parseMafRate(const char* resp, ObdData& data);
  static bool parseThrottlePos(const char* resp, ObdData& data);
  static bool parseTimingAdvance(const char* resp, ObdData& data);
  static bool parseFuelLevel(const char* resp, ObdData& data);
  static bool parseBatteryVoltage(const char* resp, ObdData& data);

  BLERemoteCharacteristic* txChar_ = nullptr;
  BLERemoteCharacteristic* rxChar_ = nullptr;

  char responseBuffer_[OBD_MAX_RESPONSE];
  size_t responseLength_ = 0;
  volatile bool responseReady_ = false;
  volatile bool abortPending_ = false;

  ObdConnectionState obdState_ = ObdConnectionState::Disconnected;
  ObdData obdData_;
  ElmState elmState_ = ElmState::InitATZ;
  uint8_t initStep_ = 0;
  uint8_t initRetries_ = 0;
  uint32_t lastCmdMs_ = 0;
  uint32_t cmdStartMs_ = 0;
  bool demoMode_ = false;
  uint32_t lastDemoUpdateMs_ = 0;

  // Demo driving simulation state machine
  enum class DemoPhase : uint8_t {
    Idle,
    Accelerating,
    ShiftCoast,
    Accelerating2,
    ShiftCoast2,
    Cruising,
    Decelerating,
    Stopping,
  };
  DemoPhase demoPhase_ = DemoPhase::Idle;
  uint32_t demoPhaseStartMs_ = 0;

  static constexpr const char* initCommands_[] = {
    "ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATH0\r", "ATSP0\r"
  };

  PidEntry pidQueue_[10];  // PIDs we poll
  uint8_t pidCount_ = 0;
  uint8_t currentPid_ = 0;

  BLESecurity security_;
  SecurityCallbacks securityCallbacks_;
  static BleObdCentral* activeInstance_;
};

#endif // BLE_OBD_CENTRAL_H
