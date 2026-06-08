# Análise Técnica: Suporte OBD-II via Bluetooth Low Energy (BLE) no Firmware ATS Mini

> **Data**: 08/06/2026
> **Propósito**: Análise completa de viabilidade para adicionar conectividade OBD-II via BLE
> **Escopo**: Apenas análise — nenhuma implementação foi realizada

---

## 1. Resumo Executivo

| Item | Status |
|------|--------|
| **Viável?** | **SIM** — completamente viável |
| **Grau de dificuldade** | Médio (7/10) |
| **Risco de regressão** | Baixo (sistema de modos BLE isolado) |
| **Impacto em RAM (DRAM)** | ~4-8 KB adicionais (de ~180KB disponíveis) |
| **Impacto em PSRAM** | ~1.5-4.5 KB (de 8MB OPI / 2MB QSPI) |
| **Impacto em Flash** | ~9-18 KB (de 3MB disponíveis por slot OTA) |
| **Impacto em CPU** | Mínimo (polling OBD cabe no delay(5) do loop) |
| **Infraestrutura BLE reutilizável** | **SIM** — BleCentral, BlePeripheral, BleMode já existem |
| **Conflitos identificados** | Nenhum crítico |

### Fator Crítico de Sucesso

O ATS Mini **já possui uma implementação BLE funcional e madura**, incluindo:
- Classe base `BleCentral` com máquina de estados scan→connect (pronta para OBD)
- Classe base `BlePeripheral` para modo servidor GATT
- `BleMode.cpp` como dispatcher entre modos BLE
- Mecanismo de `bleConsumeAbortPending()` para cooperação no loop principal

Isso reduz drasticamente o esforço — não é preciso implementar BLE do zero, apenas **especializar** a infraestrutura existente.

---

## 2. Arquitetura do Firmware (Mapeamento Completo)

### 2.1 Plataforma

| Característica | Valor |
|----------------|-------|
| **MCU** | ESP32-S3 (Xtensa LX7 dual-core) |
| **Framework** | Arduino-ESP32 v3.3.8 (ESP-IDF v5.x) |
| **BLE Stack** | NimBLE (embutido no Arduino core para ESP32-S3) |
| **Clock CPU** | 80MHz (configurável para 240MHz, mas mantido baixo por RFI) |
| **Flash** | 8MB QIO |
| **PSRAM (OSPI)** | 8MB Octal PSRAM (perfil `esp32s3-ospi`) |
| **PSRAM (QSPI)** | 2MB QSPI PSRAM (perfil `esp32s3-qspi`) |
| **PSRAM é obrigatória** | SIM — firmware trava se não detectada |
| **Display** | ST7789, 170×320 (rotacionado para 320×170) |
| **Rádio** | SI4732 via I2C a 800kHz |

### 2.2 Estrutura de Arquivos

```
ats-mini/                   ← Diretório raiz do firmware
├── ats-mini.ino            ← Entry point: setup() + loop()
├── Common.h                ← Definições centrais, pinos, modos, tipos
├── BleMode.h / .cpp        ← Dispatcher BLE (seleciona modo ativo)
├── BleCentral.h / .cpp     ← Classe base BLE Central (scan→connect state machine)
├── BleHidCentral.h / .cpp  ← Central HID (controle remoto)
├── BlePeripheral.h / .cpp  ← Classe base BLE Peripheral (servidor GATT)
├── BleUartPeripheral.h/.cpp← Periférico UART (serial BLE, Nordic UART Service)
├── Draw.h / .cpp           ← Sistema de desenho (primitivas + composição)
├── Layout-Default.cpp      ← Layout padrão do rádio
├── Layout-SMeter.cpp       ← Layout alternativo com S-meter grande
├── Menu.h / .cpp           ← Sistema de menus, comandos, handlers
├── Network.cpp             ← WiFi, servidor web, NTP
├── Storage.h / .cpp        ← NVS preferences + LittleFS
├── Remote.h / .cpp         ← Protocolo de comando remoto (USB/BLE)
├── Button.h / .cpp         ← Debounce de botão
├── Rotary.h / .cpp         ← Decodificação de encoder rotativo
├── Themes.h / .cpp         ← Sistema de temas de cores (9 temas)
├── Utils.h / .cpp          ← SSB patch, mute, sleep, clock
├── Station.h / .cpp        ← RDS, nomes de estação
├── Battery.h / .cpp        ← Monitoramento de bateria
├── EIBI.h / .cpp           ← Schedule EiBi
├── Scan.h / .cpp           ← Scanner de espectro
├── About.cpp               ← Tela de informações
├── SI4735-fixed.h          ← Wrapper SI4735 com correções de bugs RDS
├── patch_init.h            ← Patch SSB (binary blob 57KB em flash)
├── tft_setup.h             ← Configuração do display (pinos, driver)
├── partitions.csv          ← Tabela de partições
├── sketch.yaml             ← Perfis de build (Arduino CLI)
├── build_opt.h             ← Flags de compilação
└── Makefile                ← Sistema de build
```

### 2.3 Tarefas FreeRTOS

**Não há tarefas FreeRTOS explícitas.** O firmware roda integralmente no modelo cooperativo Arduino:

| Task | Núcleo | Descrição |
|------|--------|-----------|
| `loopTask` (Arduino) | Core 1 | Todo o firmware: rádio, display, BLE, WiFi, menus |
| WiFi/BT (ESP-IDF) | Core 0 | Gerenciamento interno da pilha de rádio |
| NimBLE stack | Core 0 | Camada BLE (controller + host) |

O loop principal roda a ~200Hz (`delay(5)` no final) multiplexando todos os subsistemas via `millis()`.

### 2.4 Interrupções

**Apenas uma ISR no código do usuário:**

| ISR | Pinos | Gatilho | Finalidade |
|-----|-------|---------|------------|
| `rotaryEncoder()` | GPIO1, GPIO2 | CHANGE | Decodificação do encoder rotativo |

Não há ISR do SI4732, do display, ou do BLE. O BLE usa a pilha NimBLE em background no Core 0.

### 2.5 Timers

**Não há timers de hardware explícitos.** Todo timing é feito via `millis()` no loop:

| Temporizador | Intervalo | Propósito |
|-------------|-----------|-----------|
| RSSI/SNR | 200ms | Qualidade do sinal |
| Display RSSI | 1.2s | Atualização na tela |
| RDS | 250ms | Radio Data System |
| Schedule | 2s | Identificação de frequência |
| NTP | 60s | Sincronização de hora |
| Background refresh | 5s | Refresh periódico da tela |
| Loop delay | 5ms | Yield mínimo |

---

## 3. Infraestrutura BLE Existente (Reutilizável)

### 3.1 Hierarquia de Classes

```
BlePeripheral (servidor GATT)
  └── BleUartPeripheral (serviço UART - Nordic UART Service)

BleCentral (cliente GATT - máquina de estados)
  └── BleHidCentral (HID - controles remotos)

BleMode (dispatcher)
  ├── BLE_OFF = 0
  ├── BLE_ADHOC = 1 → BleUartPeripheral
  └── BLE_HID = 2   → BleHidCentral
```

### 3.2 Capacidades do BleCentral (base para OBD)

Máquina de estados completa:
```
Idle → Started → PendingScan → Scanning → PendingConnect → Connecting → Connected
                                                                              ↓
                                                                        Disconnecting
                                                                              ↓
                                                                        PendingScan (reconexão)
```

**Operações GATT client suportadas:**
- Scan com filtro por UUID de serviço
- Connect com retry (MAX_SCAN_ATTEMPTS=3)
- Descoberta de serviços: `client->getServices()`
- Leitura de características: `characteristic->readValue()`
- Subscrição de notificações: `characteristic->subscribe()`
- Descoberta de descritores
- Disconnect com timeout de 500ms

### 3.3 Capacidades do BlePeripheral (base para servidor OBD)

- `BLEDevice::init()` + `createServer()`
- `createService()` + `createCharacteristic()`
- Advertising com UUIDs de serviço
- Callbacks de connect/disconnect
- MTU negociável (23-517 bytes)

### 3.4 Modos BLE Atuais

| Modo | Direção | UUIDs | Finalidade |
|------|---------|-------|------------|
| `BLE_ADHOC` | Peripheral | UART Service `6E400001-...` | Serial bridge para controle remoto |
| `BLE_HID` | Central | HID Service `0x1812` | Conexão com knobs Bluetooth |

**Ambos os modos são mutuamente exclusivos** — controlados por `bleModeIdx` (default: `BLE_OFF`).

### 3.5 O Que Falta para OBD

| Funcionalidade | Status |
|----------------|--------|
| Conexão BLE Central com adaptador OBD | Precisa extender `BleCentral` |
| Descoberta de serviço OBD (UUID 0xFFF0/0xFFF1) | Não implementado |
| Escrita em característica (enviar comando AT) | Precisa implementar |
| Parsing de resposta ELM327 | Não implementado |
| Pooling de OBD PIDs | Não implementado |
| Modo `BLE_OBD` | Não existe |

---

## 4. Análise do Display (Para Dados OBD)

### 4.1 Especificações

| Parâmetro | Valor |
|-----------|-------|
| **Driver** | ST7789 |
| **Resolução** | 170×320 (nativa), 320×170 (após rotação) |
| **Cores** | RGB565 (16-bit) |
| **Framebuffer** | TFT_eSprite 320×170 = 108.800 bytes (~106KB em PSRAM) |
| **Framework UI** | Custom (hand-drawn com TFT_eSPI) |
| **Redraw** | `fillSprite()` + `pushSprite(0,0)` = full frame |
| **Widgets** | **Não existem** — tudo é desenhado manualmente |
| **Overlays** | Apenas `drawMessage()` para texto transitório |
| **Sistema de páginas** | Comando `currentCmd` + layout `uiLayoutIdx` |
| **Toque** | Não suportado |

### 4.2 Mapa de Tela (Coordenadas Fixas)

```
0                   104 150       237 288 319
├──────────────────────────────────────────┤ 0   ← Status Bar: Save(90), BLE(104),
│    Banda      BLE  BAND/MODE    WiFi Bat │         BAND/MODE(150), BLE(104),
│                                          │         WiFi(237), Battery(288)
├──────────────────────────────────────────┤
│                                          │
│                                          │
│           FM     102.7    MHz            │ 62  ← Frequência (fonte Orbitron 24pt)
│                                          │
│                                          │
│           ◄── Station Name ──►           │ 94  ← RDS/Station Name
├──────────────────────────────────────────┤
│                                          │
│ S=8  ████████████████                    │ 135 ← S-meter / Status Text / RDS Text
│       ◄── Frequency Scale ──►            │
└──────────────────────────────────────────┘ 170
```

### 4.3 Área Disponível para Widgets OBD

**Opção A — Área de Status (y=135-170, ~35px):**
- Atualmente usada para RDS text, S-meter, ou escala
- Comporta 1-2 linhas de texto em fonte pequena (font 2 = 8pt)
- Ex: `RPM: 2450  VEL: 88 km/h`

**Opção B — Substituir layout existente:**
- Criar `UI_OBD` como terceiro layout (seguindo padrão UI_DEFAULT / UI_SMETER)
- Acesso por menu Settings → UI Layout → OBD
- Tela inteira dedicada a dados OBD

**Opção C — Widgets sobrepostos na tela principal:**
- Reutilizar `drawMessage()` ou criar overlay semi-transparente
- Mostrar dados OBD no canto inferior por alguns segundos

---

## 5. Memória

### 5.1 Tabela de Partições

| Partição | Offset | Tamanho | Finalidade |
|----------|--------|---------|------------|
| nvs | 0x9000 | 20KB | NVS padrão (WiFi cal, bonding BLE) |
| otadata | 0xE000 | 8KB | Metadados OTA |
| app0 | 0x10000 | **3MB** | Slot OTA 0 |
| app1 | 0x310000 | **3MB** | Slot OTA 1 |
| littlefs | 0x610000 | **~1.8MB** | Arquivos EiBi, config |
| settings | 0x7E0000 | **64KB** | NVS de preferências |
| coredump | 0x7F0000 | 64KB | Core dump |

### 5.2 Consumo de DRAM Interna (Crítico)

| Componente | DRAM Est. | Notas |
|------------|-----------|-------|
| NimBLE Controller | 20-35 KB | Rádio + LL scheduler |
| NimBLE Host (GATT) | 10-20 KB | GATT DB, L2CAP, SMP |
| WiFi stack | 40-60 KB | LWIP + driver (quando ativo) |
| AsyncTCP + WebServer | 16-32 KB | Buffers por conexão |
| FreeRTOS kernel | 12-16 KB | Tasks, filas, semáforos |
| Task stacks | 24-36 KB | Loop, WiFi, BLE, idle, timer |
| TFT_eSPI buffers | 2-4 KB | DMA / line buffers |
| Globals/statics | 5-10 KB | Bands, memories, scan |
| **Total estimado** | **~130-210 KB** | |
| **Livre (BLE ativo, WiFi off)** | **~120-150 KB** | |
| **Livre (BLE + WiFi ativos)** | **~90-120 KB** | |

### 5.3 Consumo de PSRAM

| Componente | PSRAM |
|------------|-------|
| TFT framebuffer (sprite 320×170) | 106 KB |
| TFT_eSPI buffers internos | ~4 KB |
| Alocações FreeRTOS heap (grandes) | variável |
| **Total estimado** | **~150-300 KB** |
| **Livre (OSPI 8MB)** | **~7.7 MB** |
| **Livre (QSPI 2MB)** | **~1.7 MB** |

### 5.4 Impacto Estimado do OBD-II BLE

| Recurso | DRAM Interna | PSRAM | Flash |
|---------|-------------|-------|-------|
| Serviço GATT OBD + 2 chars | ~2-3 KB | — | ~1-2 KB |
| Buffer de comando (512B) | — | 0.5 KB | — |
| Cache de resposta PID | — | 1-4 KB | — |
| Máquina de estados ELM327 | ~0.5 KB | — | ~4-8 KB |
| Parser de protocolo OBD | ~0.5 KB | — | ~2-4 KB |
| NimBLE overhead (1 serviço) | ~1-2 KB | — | — |
| **Total** | **~4-8 KB** | **~1.5-4.5 KB** | **~9-18 KB** |

**Conclusão**: Impacto insignificante — DRAM livre >120KB com BLE ativo.

---

## 6. Análise de Conflitos

### 6.1 I2C (SI4732)

| Característica | Valor |
|----------------|-------|
| **Barramento** | Wire (dedicado ao SI4732) |
| **Frequência** | 800kHz (fast mode) |
| **GPIOs** | SDA=GPIO18, SCL=GPIO17/GPIO8 |
| **Endereço** | 0x11 (8-bit) / 0x22 (7-bit) |
| **Conflito com BLE?** | **NÃO** — I2C e BLE usam periféricos diferentes no ESP32-S3 |

### 6.2 DSP / Áudio

| Característica | Valor |
|----------------|-------|
| **DSP no ESP32?** | **NÃO** — todo DSP é interno ao SI4732 |
| **Áudio digital?** | **NÃO** — saída analógica direta do SI4732 |
| **Buffers de áudio?** | **NÃO** — zero buffers de áudio no ESP32 |
| **Conflito com BLE?** | **NÃO** — caminho de áudio é totalmente independente |

### 6.3 Interrupções

| Característica | Valor |
|----------------|-------|
| **ISRs existentes** | Apenas rotaryEncoder (GPIO1, GPIO2, CHANGE) |
| **ISR do SI4732** | **NÃO** — nenhum pino de interrupção conectado |
| **Conflito com BLE?** | **NÃO** — ISR é extremamente leve (só incrementa contador) |

### 6.4 Timers / Watchdog

| Característica | Valor |
|----------------|-------|
| **Hardware timers** | Não usados |
| **Watchdog** | Não configurado explicitamente |
| **Delay mínimo** | `delay(5)` no loop principal = yield de 5ms |
| **Conflito com BLE?** | **NÃO** — o delay(5) dá folga para BLE processar |

### 6.5 WiFi

| Característica | Valor |
|----------------|-------|
| **WiFi default** | **OFF** (NET_OFF) |
| **Modos WiFi** | Off, AP Only, AP+Connect, Connect, Sync Only |
| **Coexistência configurada?** | **NÃO** — nenhuma configuração `esp_coex_*` |
| **Conflito com BLE?** | **BAIXO** — WiFi é off por padrão. Se ligado, coexistência hardware-level funciona, mas sem priorização |

### 6.6 Display

| Característica | Valor |
|----------------|-------|
| **Interface** | SPI (LilyGo) ou 8-bit parallel (PCB) |
| **SPI freq** | 40MHz |
| **DMA** | Não configurado |
| **Conflito com BLE?** | **NÃO** — SPI/Parallel são barramentos diferentes do BLE RF |

---

## 7. Arquitetura Recomendada

### 7.1 Diagrama de Arquitetura

```
┌─────────────────────────────────────────────────────────────┐
│                    ATS Mini Firmware                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐    ┌─────────────────────────────────────┐ │
│  │  BleMode.cpp  │    │  Main Loop (loopTask, Core 1)      │ │
│  │  (dispatcher) │    │                                     │ │
│  │               │    │  ┌─────┐ ┌──────┐ ┌──────┐ ┌────┐ │ │
│  │ bleModeIdx ───┼─── │  │Radio│ │BLE   │ │WiFi  │ │UI  │ │ │
│  │  0=OFF        │    │  │     │ │Loop()│ │Tick()│ │    │ │ │
│  │  1=ADHOC      │    │  │     │ │      │ │      │ │    │ │ │
│  │  2=HID        │    │  └─────┘ └──────┘ └──────┘ └────┘ │ │
│  │  3=OBD ◄── NOVO│   │              ↕                      │ │
│  └──────┬───────┘    │    ┌──────────────────┐             │ │
│         │            │    │  drawScreen()     │             │ │
│         │            │    │  ┌──────────────┐ │             │ │
│         ▼            │    │  │UI_DEFAULT    │ │             │ │
│  ┌─────────────┐     │    │  │UI_SMETER     │ │             │ │
│  │BleObdCentral│◄────┘    │  │UI_OBD ◄── NOVO│ │             │ │
│  │(extende     │          │  └──────────────┘ │             │ │
│  │ BleCentral) │          └──────────────────┘             │ │
│  └─────────────┘                                           │ │
│         │                                                  │ │
│         ▼                                                  │ │
│  ┌──────────────────────┐                                  │ │
│  │ Adaptador OBD-II BLE │                                  │ │
│  │ (ELM327 / vLinker /  │                                  │ │
│  │  OBDLink)            │                                  │ │
│  └──────────────────────┘                                  │ │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 Visão Geral das Arquiteturas

#### Arquitetura A: Tela Dedicada OBD (Recomendada)
- **Descrição**: Terceiro layout (`UI_OBD`) acessado via Settings → UI Layout
- **Vantagens**: Tela cheia para dados, sem competição com rádio
- **Complexidade**: Média
- **Implementação**: `Layout-OBD.cpp` seguindo padrão de Layout-Default.cpp

#### Arquitetura B: Widgets Sobrepostos
- **Descrição**: Mini-widgets OBD na tela do rádio (ex: RPM no canto inferior)
- **Vantagens**: Rádio + OBD simultâneos
- **Complexidade**: Alta (precisa redesenhar layout existente)
- **Implementação**: Modificar Layout-Default.cpp e Layout-SMeter.cpp

#### Arquitetura C: Modo Dashboard Automotivo
- **Descrição**: Modo full-screen com gauges animados (RPM, velocidade, temperatura)
- **Vantagens**: Experiência premium
- **Complexidade**: Muito Alta (precisa sistema de gauges do zero)
- **Implementação**: `Layout-OBD.cpp` com gráficos customizados

### 7.3 Arquitetura Recomendada: Combinada A+B

**Recomendação**: Implementar **Arquitetura A (tela dedicada)** primariamente, com suporte a **widgets opcionais** na tela principal via configuração.

1. Criar `BleObdCentral` estendendo `BleCentral`
2. Adicionar modo `BLE_OBD = 3`
3. Criar `Layout-OBD.cpp` para tela dedicada
4. Opcionalmente adicionar mini-widgets no Layout-Default

---

## 8. Compatibilidade com Adaptadores OBD-II BLE

### 8.1 Perfil de Conexão

A maioria dos adaptadores OBD-II BLE opera como **periférico BLE** (GATT server) com o seguinte padrão:

| Adaptador | Serviço UUID | Característica RX (escrita) | Característica TX (notificação) |
|-----------|-------------|---------------------------|-------------------------------|
| **ELM327 BLE** | 0xFFF0 | 0xFFF1 (write) | 0xFFF1 (notify) |
| **vLinker BLE** | 0xFFF0 | 0xFFF2 (write) | 0xFFF1 (notify) |
| **OBDLink CX** | 0xFFF0 | 0xFFF2 (write) | 0xFFF1 (notify) |
| **OBDLink MX+** | Custom | Custom | Custom |
| **Clones ELM327** | 0xFFF0 (ou SPP) | 0xFFF1 (write) | 0xFFF1 (notify) |

**Protocolo comum**: Enviar comando AT/pedido PID via escrita na característica RX, ler resposta via notificação na característica TX.

### 8.2 Estratégia de Descoberta

```
1. Scan BLE por dispositivos com UUID de serviço 0xFFF0
2. Conectar ao primeiro dispositivo encontrado
3. Descobrir características
4. Identificar RX (propriedade WRITE) e TX (propriedade NOTIFY)
5. Enviar ATZ (reset), aguardar resposta
6. Iniciar polling de PIDs
```

---

## 9. Estimativa de Esforço

### 9.1 Classificação de Itens

| Item | Dificuldade | Esforço | Dependências |
|------|------------|---------|--------------|
| Definição do modo BLE_OBD | Fácil | 15 min | — |
| Classe BleObdCentral (scan+conect) | Médio | 4-6h | BleCentral |
| Parser de protocolo ELM327 | Médio | 6-8h | — |
| Polling de PIDs OBD | Médio | 3-4h | Parser ELM327 |
| Tela OBD dedicada (Layout-OBD.cpp) | Médio | 4-6h | Draw.h / Layout-SMeter.cpp |
| Widgets OBD na tela principal | Difícil | 6-8h | Layout-Default.cpp |
| Indicador de status OBD (BLE icon) | Fácil | 1h | Draw.cpp |
| Salvamento de dados (DTC, logs) | Médio | 4-6h | Storage.cpp |
| Alertas de temperatura | Médio | 3-4h | Sistema de alertas |
| Monitoramento em tempo real | Difícil | 8-12h | Tudo acima |
| **Total estimado** | | **~40-56h** | |

### 9.2 Arquivos que Precisariam Ser Modificados

| Arquivo | Tipo de Mudança |
|---------|----------------|
| `Common.h` | Adicionar `#define BLE_OBD 3` |
| `BleMode.h` | Adicionar declarações `bleObdInit/Loop/Status` |
| `BleMode.cpp` | Adicionar caso `BLE_OBD` no dispatcher |
| `BleCentral.h` | (reutilizado como base) |
| `BleCentral.cpp` | (reutilizado como base) |
| **`BleObdCentral.h`** | **NOVO** — extensão de BleCentral para OBD |
| **`BleObdCentral.cpp`** | **NOVO** — scan, connect, parser ELM327, PID polling |
| `Menu.h` | Adicionar `UI_OBD` ao enum de layouts |
| `Menu.cpp` | Adicionar "OBD" ao `bleModeDesc[]`, handler de layout |
| `Draw.h` | Adicionar declarações de funções OBD |
| `Draw.cpp` | Adicionar `drawObdIndicator()` |
| **`Layout-OBD.cpp`** | **NOVO** — layout dedicado OBD |
| `Makefile` | Adicionar novos arquivos .cpp/.h |
| `sketch.yaml` | Possível nova dependência (se usar biblioteca OBD) |

---

## 10. Cenário de Uso Típico

```
Usuário liga ATS Mini
  → Rádio toca FM 102.7
  → Usuário vai em Settings → Bluetooth → "OBD"
  → Firmware inicia scan BLE
  → Encontra adaptador "OBDII" (ELM327 BLE)
  → Conecta automaticamente
  → Envia ATZ, recebe "ELM327 v1.5"
  → Inicia polling de PIDs:
      → 0x0C (RPM) a cada 200ms
      → 0x0D (Velocidade) a cada 500ms
      → 0x05 (Temperatura) a cada 1000ms
      → 0x11 (TPS) a cada 1000ms
      → 0x42 (Tensão bateria) a cada 5000ms
  → Display mostra:

    ┌──────────────────────────────────┐
    │ FM 102.7          BLE OBD  BAT   │
    │                                  │
    │ ┌──DADOS OBD──────────────────┐ │
    │ │ RPM:   2450     ████████░░  │ │
    │ │ VEL:   88 km/h  ██████░░░░  │ │
    │ │ TEMP:  91°C     █████░░░░░  │ │
    │ │ TPS:   42%      ████░░░░░░  │ │
    │ │ BATT:  12.4V    ████████░░  │ │
    │ └──────────────────────────────┘ │
    │                                  │
    └──────────────────────────────────┘

  → Se TEMP > 100°C: display pisca "⚠ TEMP ALTA"
  → Usuário volta para tela do rádio, OBD continua em background
  → Widget opcional mostra RPM no canto da tela do rádio
```

---

## 11. Riscos e Mitigações

### 11.1 Riscos de Regressão

| Risco | Probabilidade | Impacto | Mitigação |
|-------|:-----------:|:-------:|-----------|
| BLE UART (ADHOC) parar de funcionar | Baixa | Alto | Novo modo OBD é separado, não altera ADHOC |
| HID (controle remoto) parar de funcionar | Baixa | Alto | Novo modo OBD é separado, não altera HID |
| Display ficar lento com polling OBD | Média | Médio | Usar `millis()` cooperativo; delay(5) suficiente |
| SI4732 perder comunicação I2C | Muito Baixa | Alto | I2C e BLE são periféricos independentes no ESP32-S3 |
| Consumo de RAM estourar | Muito Baixa | Alto | DRAM livre >120KB; OBD adiciona ~4-8KB |
| Conflito de UUIDs BLE | Baixo | Médio | Usar UUIDs específicos OBD-II (0xFFF0) |
| Watchdog timeout | Muito Baixa | Médio | Polling OBD não bloqueia; usa chamadas não-blocantes |
| SSB patch loading atrasar conexão BLE | Baixa | Baixo | SSB patch é carregado uma vez, via I2C bulk |

### 11.2 Limitações de Hardware

| Limitação | Impacto | Contorno |
|-----------|---------|----------|
| CPU a 80MHz | Polling OBD mais lento | 240MHz disponível se necessário (comentado no .ino) |
| Sem task FreeRTOS dedicada | Polling OBD no mesmo loop do rádio | Delay(5) + millis() cooperativo é suficiente para 2-3 PIDs/s |
| Display 320×170 pequeno | Pouco espaço para dados | Tela dedicada OBD como terceiro layout |
| BLE e WiFi compartilham antena | Degradação se ambos ativos | WiFi OFF por padrão; usuário escolhe |

**Nenhuma limitação de hardware torna o projeto inviável.**

---

## 12. Comportamento Detalhado

O sistema de modos BLE atual opera em três estados mutuamente exclusivos:

### BleMode.cpp (linhas 39-51) — Inicialização:
```cpp
void bleInit(uint8_t bleMode) {
    bleStop();
    switch(bleMode) {
        case BLE_ADHOC: BLESerial.begin(RECEIVER_NAME); break;
        case BLE_HID:   BLEHid.begin(RECEIVER_NAME);     break;
        // NOVO: case BLE_OBD: BLEObd.begin(); break;
    }
}
```

### BleMode.cpp (linhas 54-93) — Loop principal:
```cpp
int bleLoop(uint8_t bleMode) {
    // ADHOC: serial bridge já implementado
    // HID:   HID central já implementado
    // NOVO:  BLEObd.loop() → poll PIDs → atualizar dados
}
```

### ats-mini.ino (linha 775) — Integração no loop principal:
```cpp
int ble_event = bleLoop(bleModeIdx);  // JÁ INTEGRADO!
needRedraw |= !!(ble_event & REMOTE_CHANGED);
```

**O loop principal já chama `bleLoop()` a cada iteração** — adicionar um novo modo OBD é apenas questão de implementar o case no dispatcher.

---

## 13. Conclusão e Recomendação Final

### Viabilidade: **COMPLETAMENTE VIÁVEL**

### Fatores Decisivos

| Fator | Avaliação |
|-------|-----------|
| **BLE stack já existe** | ✅ BleCentral, BlePeripheral, BleMode maduros |
| **PSRAM abundante** | ✅ 8MB OPI (ou 2MB QSPI) — folga enorme |
| **DRAM suficiente** | ✅ 120-150KB livres com BLE ativo |
| **CPU suficiente** | ✅ 80MHz, polling OBD cabe no delay(5) |
| **I2C não conflita** | ✅ Barramento dedicado SI4732 |
| **WiFi não conflita** | ✅ WiFi off por padrão |
| **Display adaptável** | ✅ Padrão de layouts já estabelecido |
| **Sem ISR conflict** | ✅ Apenas ISR do encoder |
| **Sem watchdog** | ✅ Não há watchdog configurado |
| **SI4732 sem timing crítico** | ✅ RDS/RSSI toleram delays |

### Arquitetura Recomendada

**Combinada A+B** = Tela dedicada OBD + Widgets opcionais

1. **Camada BLE**: `BleObdCentral` extendendo `BleCentral` 
2. **Camada Protocolo**: Parser ELM327 + PID polling state machine
3. **Camada Display**: `Layout-OBD.cpp` + indicador de status
4. **Camada Dados**: Cache de PIDs + persistência opcional

### Prioridade de Implementação

1. 🔴 `BleObdCentral` (conexão + comunicação)
2. 🔴 Parser ELM327 + PID polling
3. 🟡 `Layout-OBD.cpp` (tela dedicada)
4. 🟡 Indicador de status BLE OBD
5. 🟢 Widgets na tela principal
6. 🟢 Alertas de temperatura
7. 🟢 Salvamento de logs

### Recomendação Final

**Siga em frente com a implementação.** O firmware ATS Mini não apenas suporta OBD-II BLE como já possui 60-70% da infraestrutura necessária. O esforço estimado é de **40-56 horas** para uma implementação completa e robusta.

Os benefícios incluem:
- Transformar o ATS Mini em um dashboard automotivo + rádio
- Reutilizar tela, bateria, encoder e BLE já existentes
- Adicionar funcionalidade sem precedentes para um rádio de bolso SI4732

---

## 14. Glossário

| Termo | Definição |
|-------|-----------|
| **BleCentral** | Classe base para modo BLE Central (cliente GATT) |
| **BleHidCentral** | Especialização para HID (controles remotos) |
| **BlePeripheral** | Classe base para modo Peripheral (servidor GATT) |
| **BleUartPeripheral** | Especialização para UART serial sobre BLE |
| **BleMode** | Dispatcher dos modos BLE no firmware |
| **ELM327** | Protocolo padrão para comunicação com adaptadores OBD-II |
| **NimBLE** | Pilha BLE open-source (Apache NimBLE) usada pelo ESP32 Arduino |
| **PID** | Parameter ID — código que identifica um parâmetro OBD (ex: 0x0C = RPM) |
| **PSRAM** | Pseudostatic RAM — memória externa no ESP32-S3 |
| **SI4732** | Chip de rádio DSP da Skyworks (antiga Silicon Labs) |
| **ST7789** | Controlador de display TFT colorido |
| **TFT_eSPI** | Biblioteca Arduino para displays TFT |
| **TFT_eSprite** | Framebuffer off-screen da biblioteca TFT_eSPI |

---

*Relatório gerado por Prometheus (OhMyOpenCode) em 08/06/2026.*
*Nenhuma implementação foi realizada — apenas análise técnica.*
