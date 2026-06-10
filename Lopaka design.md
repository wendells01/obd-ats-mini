
// [BEGIN lopaka generated]
#include "Org_01.h"

void drawMain(void) {
    tft.fillScreen(0x73AE);
    // Frame_Signal_01
    tft.fillRoundRect(6, 114, 74, 39, 5, 0x3A96);
    // Frame_Engine_03
    tft.fillRoundRect(129, 217, 50, 12, 3, 0x3A96);
    // Frame_Signal_03
    tft.fillRoundRect(6, 146, 22, 12, 5, 0x4B1);
    // Frame_Engine_01
    tft.fillRoundRect(87, 199, 47, 30, 3, 0x4B1);
    // Frame_Signal_02
    tft.fillRoundRect(9, 127, 69, 24, 2, 0x0);
    // Frame_Engine_02
    tft.fillRoundRect(89, 205, 40, 22, 2, 0x0);
    // Frame_Speed_02
    tft.fillRoundRect(5, 43, 228, 67, 5, 0x3A96);
    // Value_RSSI
    tft.setTextColor(0xFFFF);
    tft.setTextSize(3);
    tft.setFreeFont(&Org_01);
    tft.drawString("-88", 11, 132);
    // Frame_Volt_02
    tft.fillRoundRect(7, 163, 74, 30, 3, 0x4B1);
    // Frame_Volt_01
    tft.fillRoundRect(32, 157, 49, 13, 5, 0x3A96);
    // Frame_Volt_03
    tft.fillRoundRect(10, 167, 68, 24, 2, 0x0);
    // Unit_Signal
    tft.setTextSize(1);
    tft.drawString("dBm", 61, 132);
    // Frame_Speed_03
    tft.fillRoundRect(164, 105, 69, 16, 5, 0x3A96);
    // Value_Volt
    tft.setTextSize(3);
    tft.drawString("13.9", 19, 172);
    // Unit_Volt
    tft.setTextSize(1);
    tft.drawString("V", 70, 172);
    // Frame_Speed_03
    tft.fillRoundRect(9, 47, 221, 59, 5, 0x0);
    // Label_Volt
    tft.drawString("VOLTAGE", 36, 160);
    // Value_Speed
    tft.setTextSize(10);
    tft.drawString("888", 12, 51);
    // Volt copy
    tft.setTextSize(1);
    tft.drawString("SIGNAL", 11, 118);
    // Label_Engine
    tft.drawString("ENGINE", 139, 221);
    // Frame_Ignition_02
    tft.fillRoundRect(182, 199, 51, 30, 3, 0x3A96);
    // Frame_Ignition_03
    tft.fillRoundRect(189, 205, 41, 22, 2, 0x0);
    // Frame_Ignition_01
    tft.fillRoundRect(138, 199, 51, 12, 3, 0x4B1);
    // Volt copy
    tft.drawString("IGNITION", 141, 203);
    // Unit_Speed
    tft.setTextSize(2);
    tft.drawString("km/h", 185, 90);
    // Frame_Odo_04
    tft.fillRoundRect(164, 150, 67, 13, 5, 0x3A96);
    // Frame_Odo_02
    tft.fillRoundRect(87, 125, 145, 28, 5, 0x4B1);
    // Frame_Odo_01
    tft.fillRoundRect(87, 114, 69, 26, 5, 0x3A96);
    // Frame_Odo_03
    tft.fillRoundRect(92, 127, 138, 24, 5, 0x0);
    // Value_Odo
    tft.setTextSize(3);
    tft.drawString("88888.8", 98, 132);
    // Frame_Trip_01
    tft.fillRoundRect(88, 157, 69, 26, 5, 0x3A96);
    // Frame_RPM_03
    tft.fillRoundRect(89, 27, 144, 12, 5, 0x3A96);
    // Frame_RPM
    tft.fillRoundRect(4, 3, 228, 27, 5, 0x4B1);
    // Volt copy
    tft.setTextSize(1);
    tft.drawString("TRIP", 95, 160);
    // Unitt_Odo
    tft.drawString("km", 215, 142);
    // Frame_RPM_02
    tft.fillRect(14, 7, 210, 12, 0xFFFF);
    // Label_Odo
    tft.drawString("ODOMETER", 94, 118);
    // Frame_Trip_02
    tft.fillRoundRect(88, 166, 145, 29, 5, 0x4B1);
    // Frame_Trip_03
    tft.fillRoundRect(93, 169, 138, 24, 5, 0x0);
    // Value_Trip
    tft.setTextSize(3);
    tft.drawString("88888.8", 99, 174);
    // Unit_Trip
    tft.setTextSize(1);
    tft.drawString("km", 215, 184);
    // Engine_ON
    tft.setTextSize(3);
    tft.drawString("ON", 92, 209);
    // Label_Speed
    tft.setTextSize(1);
    tft.drawString("SPEED", 184, 111);
    // Ignition_ON
    tft.setTextSize(3);
    tft.drawString("ON", 192, 209);
    // Label_RPM_02
    tft.setTextSize(1);
    tft.drawString("RPM x1000 r/min", 147, 32);
    // Frame_Fuel_01
    tft.fillRoundRect(8, 199, 74, 30, 3, 0x3A96);
    // Frame_Volt_03 copy
    tft.fillRoundRect(10, 206, 69, 21, 2, 0x0);
    // Frame_Fuel_03
    tft.fillRoundRect(8, 196, 39, 10, 3, 0x4B1);
    // Label_Fuel_01
    tft.drawString("FUEL", 14, 199);
    // Menu_03
    tft.drawRoundRect(166, 232, 67, 5, 2, 0x4208);
    // Menu_01
    tft.fillRoundRect(9, 232, 67, 5, 2, 0xFFFF);
    // Menu_02
    tft.drawRoundRect(88, 232, 67, 5, 2, 0x4208);
    // Grid_RPM
    tft.setTextColor(0x24BE);
    tft.setTextSize(2);
    tft.drawString(", , , , , , , , , , , , , ,", 14, 7);
    // Label_RPM
    tft.setTextColor(0xF206);
    tft.setTextSize(1);
    tft.drawString("12", 204, 23);
    // Label_RPM
    tft.drawString("11", 191, 23);
    // Label_RPM
    tft.setTextColor(0xE8EC);
    tft.drawString("10", 172, 23);
    // Label_RPM
    tft.setTextColor(0x0);
    tft.drawString("3", 61, 23);
    // Label_RPM
    tft.drawString("4", 77, 23);
    // Label_RPM
    tft.drawString("5", 93, 23);
    // Label_RPM
    tft.drawString("6", 110, 23);
    // Label_RPM
    tft.drawString("7", 125, 23);
    // Label_RPM
    tft.drawString("8", 141, 23);
    // Label_RPM
    tft.drawString("9", 157, 23);
    // Label_RPM
    tft.drawString("2", 45, 23);
    // Label_RPM
    tft.drawString("1", 30, 23);
    // Value_Fuel
    tft.setTextColor(0xFFFF);
    tft.setTextSize(3);
    tft.drawString("3.5", 25, 209);
    // Label_RPM
    tft.setTextColor(0x0);
    tft.setTextSize(1);
    tft.drawString("0", 13, 23);
    // Unit_Fuel
    tft.setTextColor(0xFFFF);
    tft.drawString("L", 69, 209);

}
// [END lopaka generated]
