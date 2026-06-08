#ifndef BLE_MODE_H
#define BLE_MODE_H

#include "Remote.h"
#include "BleUartPeripheral.h"
#include "BleObdCentral.h"

extern BleObdCentral BLEObd;

void bleInit(uint8_t bleMode);
void bleStop();
int8_t getBleStatus();
int bleLoop(uint8_t bleMode);
bool bleConsumeAbortPending(uint8_t bleMode);

#endif
