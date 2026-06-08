# OBD Layout - Temperature Alert Overlay

## File Structure
- `Layout-OBD.cpp` contains `drawLayoutObd()` function with all OBD UI rendering
- Uses `spr` (TFT_eSprite) for all drawing operations

## Existing Patterns
- `TH.text_warn` = red warning color (0xF800)
- `Orbitron_Light_24` = large font used for RPM display
- `spr.fillRect(x, y, w, h, color)` for filled rectangles
- `spr.setFreeFont(&Orbitron_Light_24)` to set large font, `spr.setFreeFont(NULL)` to reset
- TC_DATUM = center datum for text centering
- `drawObdBar()` draws individual data bars with label, bar, value
- Coolant temp is at column 2, row 0 (y=75)

## Data Types
- `d.coolantTemp` = int8_t, °C
- `d.coolantTempValid` = bool
- Both in `ObdData` struct from `BleObdCentral.h`

## Drawing Coordinates
- RPM: y=24-58 (large text at y=24, label at y=58)
- Data rows start at y=75, spaced 25px apart (75, 100, 125)
- Column widths: colW=155, column 1 at x=5, column 2 at x=165
- Total width: 320px

## Implementation for Temp Alert
- Overlay at y=55 to y=115 (overlaps RPM/label and data rows)
- Use static bool for hysteresis (activate >100°C, clear <95°C)
- Blink: (millis()/500) & 1 for 500ms on/off cycle
- Draw red rect, then white "TEMP!" in Orbitron_Light_24, then temperature value
- Must reset free font after drawing "TEMP!" for the smaller temp value text

---

## IMPLEMENTATION COMPLETE — Jun 08 2026

### Arquivos Criados (3)
| Arquivo | Linhas | Conteúdo |
|---------|--------|----------|
| `BleObdCentral.h` | 158 | Classe `BleObdCentral : BleCentral`, struct `ObdData` (10 PIDs + *Valid flags), enum `ElmState` |
| `BleObdCentral.cpp` | 636 | Scan (0xFFF0/0xFFF1), connect, notify, ELM327 init, PID polling (10 parsers), reconnect, 30s timeout |
| `Layout-OBD.cpp` | 172 | Painel OBD: RPM grande (Orbitron), 6 barras de progresso, status, temp alert overlay |

### Arquivos Modificados (11)
`Common.h`, `BleMode.h`, `BleMode.cpp`, `Menu.h`, `Menu.cpp`, `Draw.h`, `Draw.cpp`, `Layout-Default.cpp`, `Layout-SMeter.cpp`, `Makefile`, `BleObdCentral.cpp`

### Funcionalidades Implementadas
1. **BLE_OBD=3**: novo modo Bluetooth no menu → scan → connect → ELM327 init
2. **PID Polling**: 10 PIDs (RPM, Speed, Coolant, EngineLoad, IntakeTemp, MAF, TPS, TimingAdvance, FuelLevel, BatteryVoltage)
3. **UI_OBD Layout**: tela dedicada com dados do veículo em tempo real
4. **Indicador OBD**: ícone verde/vermelho na barra de status (Default + SMeter)
5. **Mini-widget**: RPM opcional na tela do rádio (Layout-Default)
6. **Alerta Temperatura**: overlay piscante "TEMP!" quando coolant > 100°C (histerese 95°C)
7. **Reconexão Automática**: `beginScan()` quando desconecta enquanto modo OBD ativo
8. **Data Timeout**: 30s sem resposta → invalida todos os *Valid flags
9. **UUID Alternativo**: aceita adaptadores 0xFFF0 **ou** 0xFFF1

### Pendências (Hardware Necessário)
- **5.4** LittleFS logging (opcional)
- **5.6** Teste regressão ADHOC
- **5.7** Teste regressão HID

### Lições
- `quick` category é melhor que `deep` para edições pequenas e bem especificadas
- Subagentes `deep` tendem a timeout em 30min; para mudanças pontuais usar `quick`
- A base `BleCentral` já fornece scan/connect/discover — BleObdCentral só precisa sobrescrever `acceptsAdvertisement()` e `setupConnectedPeer()`
