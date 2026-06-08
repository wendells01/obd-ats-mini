# Plano de Implementação: OBD-II via BLE no ATS Mini

> **Baseado na análise técnica**: `ats-mini/obd2-ble-analise-tecnica.md`
> **Estratégia**: Entregas incrementais — cada etapa é funcional por si só
> **Estimativa total**: ~40-56h (distribuída em 5 etapas)

---

## Faseamento

### Fase 1 — Infraestrutura BLE OBD (~8-12h)
**Objetivo**: Conexão funcional com adaptador OBD-II BLE, dados no log serial.

| # | Tarefa | Arquivos | Dependência |
|---|--------|----------|-------------|
| 1.1 | Adicionar `BLE_OBD = 3` em `Common.h` | `Common.h` | — |
| 1.2 | Criar `BleObdCentral.h` estendendo `BleCentral` | `BleObdCentral.h` (NOVO) | 1.1 |
| 1.3 | Implementar scan por UUID 0xFFF0 em `BleObdCentral.cpp` | `BleObdCentral.cpp` (NOVO) | 1.2 |
| 1.4 | Implementar connect + descoberta de características | `BleObdCentral.cpp` | 1.3 |
| 1.5 | Implementar escrita (comando) + leitura (notificação) | `BleObdCentral.cpp` | 1.4 |
| 1.6 | Integrar `BleObdCentral` no `BleMode.cpp` dispatcher | `BleMode.cpp`, `BleMode.h` | 1.5 |
| 1.7 | Adicionar "OBD" ao `bleModeDesc[]` no menu | `Menu.cpp` | 1.6 |
| 1.8 | Adicionar ao `Makefile` | `Makefile` | 1.7 |

**Critério de aceite**: Ao selecionar Bluetooth → "OBD" no menu, o firmware escaneia, conecta ao adaptador OBD e exibe "OBD Connected" no display.

---

### Fase 2 — Parser ELM327 + PID Polling (~10-14h)
**Objetivo**: Interpretar comandos ELM327 e fazer polling periódico de PIDs.

| # | Tarefa | Arquivos | Dependência |
|---|--------|----------|-------------|
| 2.1 | Implementar máquina de estados ELM327 (ATZ, ATE0, etc.) | `BleObdCentral.cpp` | 1.5 |
| 2.2 | Implementar parser de resposta ELM327 (split por `\r`) | `BleObdCentral.cpp` | 2.1 |
| 2.3 | Implementar fila de PIDs com intervalos configuráveis | `BleObdCentral.cpp` | 2.2 |
| 2.4 | PID 0x0C (RPM) | `BleObdCentral.cpp` | 2.3 |
| 2.5 | PID 0x0D (Velocidade) | `BleObdCentral.cpp` | 2.3 |
| 2.6 | PID 0x05 (Temperatura motor) | `BleObdCentral.cpp` | 2.3 |
| 2.7 | PID 0x11 (TPS) | `BleObdCentral.cpp` | 2.3 |
| 2.8 | PID 0x42 (Tensão bateria veículo) | `BleObdCentral.cpp` | 2.3 |
| 2.9 | Cache de respostas com timestamp | `BleObdCentral.cpp` | 2.4-2.8 |
| 2.10 | Estrutura `ObdData` compartilhada (thread-safe via volatile) | `BleObdCentral.h` | 2.9 |

**Critério de aceite**: Dados OBD reais disponíveis na struct `ObdData` e exibíveis via `Serial.printf()`.

---

### Fase 3 — Tela OBD Dedicada (~6-8h)
**Objetivo**: Layout `UI_OBD` com painel de dados do veículo.

| # | Tarefa | Arquivos | Dependência |
|---|--------|----------|-------------|
| 3.1 | Adicionar `UI_OBD` ao enum de layouts em `Menu.h` | `Menu.h` | — |
| 3.2 | Criar `Layout-OBD.cpp` com estrutura de painel | `Layout-OBD.cpp` (NOVO) | 3.1 |
| 3.3 | Desenhar linhas de dados (RPM, VEL, TEMP, TPS, BATT) | `Layout-OBD.cpp` | 3.2, 2.10 |
| 3.4 | Desenhar barras de progresso para cada parâmetro | `Layout-OBD.cpp` | 3.3 |
| 3.5 | Adicionar indicador de status da conexão OBD | `Layout-OBD.cpp` | 3.3 |
| 3.6 | Registrar layout no `drawScreen()` em `Draw.cpp` | `Draw.cpp` | 3.2 |
| 3.7 | Adicionar opção "OBD" no menu de layout de UI | `Menu.cpp` | 3.6 |
| 3.8 | Adicionar ao `Makefile` | `Makefile` | 3.7 |

**Critério de aceite**: Ao trocar UI Layout para "OBD", a tela mostra RPM, velocidade, temperatura, TPS e tensão em tempo real.

---

### Fase 4 — Indicadores + Integração (~4-6h)
**Objetivo**: Feedback visual de status OBD na tela principal.

| # | Tarefa | Arquivos | Dependência |
|---|--------|----------|-------------|
| 4.1 | Desenhar `drawObdIndicator()` com ícone OBD (conectado/desconectado) | `Draw.cpp`, `Draw.h` | 3.6 |
| 4.2 | Adicionar `drawObdIndicator()` no Layout-Default e Layout-SMeter | `Layout-Default.cpp`, `Layout-SMeter.cpp` | 4.1 |
| 4.3 | Mini-widget OBD na área de status (y=135) quando conectado | `Layout-Default.cpp` | 2.10 |
| 4.4 | Indicador de when OBD está em segundo plano (rádio ativo) | `Draw.cpp` | 4.1 |

**Critério de aceite**: Ícone OBD aparece na barra de status quando conectado. Mini-widget opcional mostra RPM na tela do rádio.

---

### Fase 5 — Alertas + Robustez (~8-12h)
**Objetivo**: Funcionalidades avançadas e tratamento de erros.

| # | Tarefa | Arquivos | Dependência |
|---|--------|----------|-------------|
| 5.1 | Detecção de desconexão e reconexão automática | `BleObdCentral.cpp` | 2.1 |
| 5.2 | Timeout de resposta (30s sem resposta → "No Data") | `BleObdCentral.cpp` | 2.3 |
| 5.3 | Alerta de temperatura > 100°C (overlay + piscar) | `Draw.cpp`, `Layout-OBD.cpp` | 3.3 |
| 5.4 | Salvamento de dados em log no LittleFS | `Storage.cpp`, `BleObdCentral.cpp` | 2.9 |
| 5.5 | Suporte a adaptadores com UUIDs alternativos (0xFFF1, custom) | `BleObdCentral.cpp` | 1.3 |
| 5.6 | Teste de regressão: BLE_ADHOC continua funcionando | Teste manual | — |
| 5.7 | Teste de regressão: BLE_HID continua funcionando | Teste manual | — |

**Critério de aceite**: Reconexão automática, alertas visuais, logs salvos, modos BLE existentes intactos.

---

## Diagrama de Dependências

```
Fase 1 ──────────────────────────────────────────────
  └── 1.1 (Common.h)
       └── 1.2 (BleObdCentral.h)
            └── 1.3 (scan) → 1.4 (connect) → 1.5 (read/write)
                 └── 1.6 (BleMode dispatcher) → 1.7 (menu) + 1.8 (Makefile)
                      │
Fase 2 ───────────────┤
                      │
                      1.5 ──→ 2.1 (ELM327 state machine)
                                └── 2.2 (parser)
                                     └── 2.3 (PID queue)
                                          ├── 2.4 (RPM)
                                          ├── 2.5 (Speed)
                                          ├── 2.6 (Temp)
                                          ├── 2.7 (TPS)
                                          ├── 2.8 (Batt)
                                          └── 2.9 (cache) → 2.10 (ObdData)
                                               │
Fase 3 ──────────────────────────────────────────────
  │                                               │
  3.1 (Menu.h:UI_OBD)                   2.10 (ObdData)
  └── 3.2 (Layout-OBD.cpp) ←──────────────┘
       └── 3.3 (data lines) → 3.4 (bars) → 3.5 (status)
            └── 3.6 (Draw.cpp switch)
                 └── 3.7 (Menu.cpp UI layout option) + 3.8 (Makefile)
                      │
Fase 4 ───────────────┤
                      │
                 Fase 1-3 integrados
                 ├── 4.1 (drawObdIndicator)
                 ├── 4.2 (layouts)
                 ├── 4.3 (mini-widget)
                 └── 4.4 (background indicator)
                      │
Fase 5 ───────────────┤
                      │
                 Fase 2 completa
                 ├── 5.1 (reconnect)
                 ├── 5.2 (timeout)
                 ├── 5.3 (temp alert) ← Fase 3
                 ├── 5.4 (logging) ← Storage
                 ├── 5.5 (alt UUIDs)
                 ├── 5.6 (regression: ADHOC)
                 └── 5.7 (regression: HID)
```

---

## Arquivos: Resumo de Mudanças

### Novos Arquivos (3)
| Arquivo | Conteúdo |
|---------|----------|
| `BleObdCentral.h` | Classe que estende `BleCentral`, struct `ObdData`, enum `ObdStatus` |
| `BleObdCentral.cpp` | Scan 0xFFF0, connect, ELM327 parser, PID polling, reconnect |
| `Layout-OBD.cpp` | Tela dedicada com dados do veículo |

### Arquivos Modificados (10)
| Arquivo | Mudança |
|---------|---------|
| `Common.h` | +`#define BLE_OBD 3` |
| `BleMode.h` | +declarações para modo OBD |
| `BleMode.cpp` | +caso `BLE_OBD` no switch |
| `Menu.h` | +`UI_OBD` no enum |
| `Menu.cpp` | +"OBD" em bleModeDesc, +opção layout |
| `Draw.h` | +`drawObdIndicator()` |
| `Draw.cpp` | +switch para UI_OBD, +indicador |
| `Layout-Default.cpp` | +mini-widget OBD (opcional) |
| `Layout-SMeter.cpp` | +indicador OBD |
| `Makefile` | +novos arquivos no SRC e HEADERS |

### Arquivos Reutilizados (sem mudanças)
| Arquivo | Uso |
|---------|-----|
| `BleCentral.h/.cpp` | Base class herdada por BleObdCentral |
| `BlePeripheral.h/.cpp` | Não usado (OBD usa modo Central, não Peripheral) |
| `Storage.cpp` | Potencial para salvar logs (Fase 5) |

---

## Estruturas de Dados Propostas

```cpp
// BleObdCentral.h

enum class ObdConnectionState : uint8_t {
    Disconnected,
    Scanning,
    Found,
    Connecting,
    Initializing,   // Enviando ATZ, ATE0, etc
    Connected,
    Disconnecting,
};

struct ObdData {
    // Timestamp da última atualização (millis())
    uint32_t updated;
    
    // PIDs padrão
    uint16_t rpm;           // 0x0C, resolução 0.25 RPM
    uint8_t  speed;         // 0x0D, km/h
    int8_t   coolantTemp;   // 0x05, °C (pode ser negativo)
    uint8_t  throttlePos;   // 0x11, percentual 0-100
    float    batteryVoltage;// 0x42, volts
    
    // Flags de validade
    bool rpmValid : 1;
    bool speedValid : 1;
    bool coolantTempValid : 1;
    bool throttlePosValid : 1;
    bool batteryVoltageValid : 1;
};
```

---

## Comportamento no Loop Principal

```cpp
// Em bleLoop(), novo caso BLE_OBD:
int bleLoop(uint8_t bleMode) {
    // ... existing cases ...
    
    case BLE_OBD:
        BLEObd.loop();              // Processa estado da conexão
        if (!BLEObd.isConnected())
            return 0;
        BLEObd.update();            // Poll PIDs pendentes
        return REMOTE_CHANGED;      // Aciona redraw se necessário
}
```

```cpp
// Em loop() - já integrado via bleLoop:
int ble_event = bleLoop(bleModeIdx);  // JÁ EXISTE
needRedraw |= !!(ble_event & REMOTE_CHANGED);  // JÁ EXISTE
```

---

## Verificação e Qualidade

### A Cada Fase
- [x] Código compila sem warnings (verificado via subagentes)
- [x] Novos modos BLE (ADHOC, HID) não foram alterados

### Ao Final
- [x] `BleObdCentral.cpp/h` — scan 0xFFF0/0xFFF1, connect, notify, ELM327 init, PID polling
- [x] `Layout-OBD.cpp` — tela dedicada com RPM + 6 barras de progresso
- [x] `drawObdIndicator()` — indicador OBD na barra de status (Default + SMeter)
- [x] Reconexão automática ao desconectar (5.1)
- [x] Timeout 30s sem dados → flags *Valid = false (5.2)
- [x] Alerta temperatura > 100°C com overlay piscante (5.3)
- [x] Suporte a UUID alternativo 0xFFF1 (5.5)
- [~] Salvamento de dados em log no LittleFS (5.4) — **opcional, postergado**
- [~] Teste regressão ADHOC (5.6) — **requer hardware físico**
- [~] Teste regressão HID (5.7) — **requer hardware físico**

---

## Observações Técnicas

1. **BLE Central vs Peripheral**: OBD-II usa BLE Central (ATS Mini conecta no adaptador). `BleCentral` já é a classe base correta — `BlePeripheral` não será usada.

2. **NimBLE vs Bluedroid**: O ESP32-S3 com Arduino-ESP32 3.3.8 usa NimBLE, que suporta múltiplos clientes GATT simultâneos. Não é possível ter BLE_ADHOC e BLE_OBD ao mesmo tempo por design (modos mutuamente exclusivos).

3. **Polling vs Notificação**: OBD-II via BLE usa polling (envia comando → espera resposta). Não há notificação contínua. O intervalo mínimo recomendado entre PIDs é 50ms para não sobrecarregar o adaptador.

4. **Thread Safety**: Como tudo roda no mesmo loop cooperativo, não há concorrência real. Variáveis compartilhadas (`ObdData`) não precisam de mutex, apenas acesso atômico.

5. **Display**: O layout `UI_OBD` substitui completamente a tela do rádio. Para ver dados OBD + rádio simultaneamente, é necessário alternar entre layouts (Settings → UI Layout).

6. **Conexão BLE**: O adaptador OBD-II deve estar pareável. A primeira conexão pode exigir配对 (pairing) manual via smartphone. A implementação deve tratar `ESP_IO_CAP_NONE` (sem display/teclado para pairing).

---

## Apêndice: Exemplo de Uso ELM327

```
ATZ           → "ELM327 v1.5"           (reset)
ATE0          → "OK"                    (echo off)
ATL0          → "OK"                    (linefeeds off)
ATS0          → "OK"                    (spaces off)
ATH0          → "OK"                    (headers off)
ATSP0         → "OK"                    (auto protocol)
010C          → "41 0C 09 9A"          (RPM = 0x099A / 4 = 2450)
010D          → "41 0D 58"             (Speed = 0x58 = 88 km/h)
0105          → "41 05 5B"             (Coolant = 0x5B = 91°C)
0111          → "41 11 2A"             (TPS = 0x2A = 42%)
0142          → "41 42 7C"             (Battery = 0x7C * 0.1 = 12.4V)
```

---
