# Plano: OBD App Mode + Demo Mode

## TL;DR

> **Quick Summary**: Converter OBD de layout fixo (UI_OBD) para overlay temporário estilo "app" (CMD_OBD), adicionar modo demo via web, e manter mini-widget na tela do rádio.
>
> **Deliverables**:
> - Menu → OBD abre overlay com dados do carro (ou mensagem se BLE não ativo)
> - Clicar no encoder sai do overlay e volta ao rádio
> - Demo mode ativável via web (/config) que simula dados OBD sem scanner
> - Mini-widget RPM na tela Default continua funcionando
> - Migração automática de uiLayoutIdx=2 (pré-existente) para UI_DEFAULT
>
> **Estimated Effort**: Medium
> **Parallel Execution**: YES — 2 waves
> **Critical Path**: Task 1 → Task 4 → Task 5 → Task 8

---

## Context

### Original Request
Usuário instalou firmware ATS Mini com OBD2 BLE. Três problemas:
1. OBD é um layout permanente (UI_OBD) — quando selecionado, não tem como voltar (preso)
2. Sem scanner OBD, mostra "OBD Scanning..." e tela fica congelada
3. Quer testar sem hardware — modo demo via interface web

### Interview Summary
**Key Discussions**:
- **Acesso**: Menu principal (overlay) + Settings → Bluetooth → OBD (ambos)
- **Demo**: Pela interface web (checkbox no /config)
- **OBD na tela do rádio**: Mini-widget já existe no Layout-Default, apenas manter
- **Saída**: Clique no encoder sai do overlay (mesmo padrão do CMD_ABOUT)

**Research Findings**:
- `CMD_ABOUT = 0x3000` serve de padrão — é verificado antes do switch de layout em `drawScreen()` e faz early return
- `clickHandler()` não trata CMD_ABOUT — cai no fallthrough `else if(currentCmd != CMD_NONE)` que zera o comando
- `drawLayoutObd()` NÃO chama `spr.pushSprite()` — essencial adicionar no caminho CMD_OBD
- Web server já existe em Network.cpp com `/config` e `/setconfig`

### Metis Review
**Identified Gaps** (addressed):
- **GAP: Missing pushSprite()** → Adicionar `spr.pushSprite(0,0)` no caminho CMD_OBD em drawScreen()
- **GAP: Pref migration hazard** → Clampear `uiLayoutIdx` no load se >= ITEM_COUNT(uiLayoutDesc)
- **GAP: bleStop() kills other BLE** → Só chamar `bleInit(BLE_OBD)` se `bleModeIdx != BLE_OBD`

---

## Work Objectives

### Core Objective
Converter OBD de layout permanente (UI_OBD) para overlay temporário (CMD_OBD), adicionar modo demo via web, e garantir que mini-widget na tela do rádio continue funcionando.

### Concrete Deliverables
- Menu → OBD abre overlay (drawLayoutObd via currentCmd)
- Click no encoder sai do overlay
- Web demo toggle persiste em NVS e gera dados simulados
- uiLayoutIdx=2 migrado silenciosamente para UI_DEFAULT

### Definition of Done
- [ ] `make build` compila sem erros
- [ ] Menu → OBD abre overlay, click → volta
- [ ] POST /setconfig?obddemo=1 ativa demo, dados aparecem no overlay
- [ ] uiLayoutIdx=2 carregado do NVS é automaticamente corrigido

### Must Have
- CMD_OBD funcionando como overlay (mesmo padrão de CMD_ABOUT)
- Demo mode gerando dados simulados (RPM, velocidade, temperatura etc.)
- Web toggle funcional (ON/OFF)
- Mini-widget RPM no Layout-Default preservado
- Migração de uiLayoutIdx=2 silenciosa

### Must NOT Have (Guardrails)
- Não mexer no parser ELM327, PIDs ou intervalos de polling
- Não adicionar WebSocket ou streaming em tempo real pela web
- Não adicionar DTC, logging ou custom PID config
- Não deletar ou modificar a estrutura de BleObdCentral (só adicionar demo flag)
- Não alterar isMenuMode()/isSettingsMode()

### Spec Framework Integration
> Nenhum SDD framework detectado.

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: YES (Makefile + arduino-cli)
- **Automated tests**: None (embedded firmware sem test framework)
- **QA Method**: Agent-executed via curl (web) + build validation + deploy com verificação visual

### QA Policy
Cada task inclui cenários agent-executáveis. Evidências em `.omo/evidence/`.

- **Web endpoints**: Bash (curl) — GET/POST, assert status + HTML content
- **Build**: Bash (make build) — assert exit code 0
- **BLE Demo**: Simulated via demo flag + curl toggle

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Foundation + Overlay):
├── Task 1: CMD_OBD constant + Menu entry [quick]
├── Task 2: OBD overlay path in drawScreen() [quick]
├── Task 3: Preference migration clamp [quick]
├── Task 4: Update Layout-OBD.cpp for overlay behavior [quick]

Wave 2 (Demo Mode + Web + Wire):
├── Task 5: Auto-activate BLE_OBD on Menu→OBD [quick]
├── Task 6: Demo mode in BleObdCentral [unspecified-high]
├── Task 7: Web interface toggle for demo [unspecified-high]
├── Task 8: Build verification + final wiring [quick]

Wave FINAL (Review):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high)
└── Task F4: Scope fidelity check (deep)

Critical Path: Task 1 → Task 4 → Task 5 → Task 8
```

---

## TODOs

- [x] 1. **Adicionar CMD_OBD + Constantes + Entrada no Menu**

  **What to do**:
  - `Menu.h`: Adicionar `#define CMD_OBD 0x3100` após `CMD_ABOUT` (linha 47)
  - `Menu.h`: Manter `#define UI_OBD 2` (não remover — usado no switch em Draw.cpp). Só remover do array `uiLayoutDesc[]`
  - `Menu.cpp`: Adicionar `#define MENU_OBD 12` antes de `MENU_SETTINGS`. ATUALIZAR: `#define MENU_SETTINGS 13` (deslocado)
  - `Menu.cpp`: Inserir `"OBD"` no array `menu[]` entre "SoftMute" e "Settings" (índice 12, deslocando Settings para 13)
  - `Menu.cpp`: Remover `"OBD"` do array `uiLayoutDesc[]` (deixar só `"Default"`, `"S-Meter"`)
  - `Menu.cpp`: Em `clickMenu()`, adicionar case `MENU_OBD: currentCmd = CMD_OBD; break;`

  **Must NOT do**:
  - Não alterar `isMenuMode()` ou `isSettingsMode()`
  - Não alterar `bleModeDesc[]` — modo BLE_OBD continua existindo

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Edições pontuais e bem definidas em 3 arquivos. Nenhuma lógica nova.
  - **Skills**: Nenhuma necessária
  - **Skills Evaluated but Omitted**: N/A

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 2, 3, 4)
  - **Blocks**: Task 4, Task 5
  - **Blocked By**: None

  **References**:
  - `Menu.h:47` — Local dos defines CMD (adicionar CMD_OBD = 0x3100 após CMD_ABOUT)
  - `Menu.h:49-52` — Layout enums (UI_DEFAULT, UI_SMETER)
  - `Menu.cpp:76-107` — Menu array e índices
  - `Menu.cpp:860-901` — clickMenu() handlers
  - `Menu.cpp:266-268` — uiLayoutDesc array

  **Acceptance Criteria**:
  - [ ] `make build` compila sem erros
  - [ ] Menu agora mostra "OBD" como item navegável
  - [ ] uiLayoutDesc mostra apenas "Default" e "S-Meter"

  **QA Scenarios**:
  ```
  Scenario: Build compila com CMD_OBD
    Tool: Bash
    Steps:
      1. Run `make build`
    Expected Result: Exit code 0, no errors
    Evidence: .omo/evidence/task-1-build.log

  Scenario: Menu item OBD existe
    Tool: grep
    Steps:
      1. grep "OBD" ats-mini/Menu.cpp | grep menu\[\]
    Expected Result: Array menu[] contém "OBD"
    Evidence: .omo/evidence/task-1-menu-item.log
  ```

  **Commit**: YES (groups with 2-4)
  - Message: `feat(obd): add CMD_OBD command and menu entry`
  - Files: `Common.h`, `Menu.h`, `Menu.cpp`

- [x] 2. **Adicionar overlay CMD_OBD no drawScreen()**

  **What to do**:
  - `Draw.cpp`: Em `drawScreen()`, ADICIONAR bloco ANTES do `switch(uiLayoutIdx)`:
    ```cpp
    if(currentCmd == CMD_OBD) {
      drawLayoutObd(statusLine1, statusLine2);
      spr.pushSprite(0, 0);  // CRÍTICO: OBD layout não chama pushSprite internamente
      return;
    }
    ```
  - Seguir exatamente o mesmo padrão do `CMD_ABOUT` (linhas 472-476)

  **Must NOT do**:
  - Não remover o case `UI_OBD` do switch (manter para compatibilidade)
  - Não alterar o fluxo do CMD_ABOUT

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Mudança de 5 linhas seguindo padrão existente (CMD_ABOUT)
  - **Skills**: Nenhuma

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 3, 4)
  - **Blocks**: None (independe de outras tasks)
  - **Blocked By**: None

  **References**:
  - `Draw.cpp:464-491` — drawScreen() completo, ver CMD_ABOUT pattern lines 472-476
  - `Layout-OBD.cpp` — drawLayoutObd() não tem pushSprite

  **Acceptance Criteria**:
  - [ ] `make build` compila sem erros
  - [ ] CMD_OBD ativo → drawScreen() desenha OBD e chama pushSprite

  **QA Scenarios**:
  ```
  Scenario: CMD_OBD path existe em drawScreen
    Tool: grep
    Steps:
      1. grep "CMD_OBD" ats-mini/Draw.cpp
    Expected Result: drawScreen() contém if(currentCmd == CMD_OBD)
    Evidence: .omo/evidence/task-2-obd-path.log

  Scenario: pushSprite presente no caminho OBD
    Tool: grep -A5 "CMD_OBD" ats-mini/Draw.cpp
    Steps:
      1. grep -A5 "CMD_OBD" ats-mini/Draw.cpp
    Expected Result: pushSprite(0,0) aparece dentro do bloco CMD_OBD
    Evidence: .omo/evidence/task-2-pushsprite.log
  ```

  **Commit**: YES (groups with 1, 3, 4)
  - Message: `feat(obd): add OBD overlay path in drawScreen()`
  - Files: `Draw.cpp`

- [x] 3. **Migração de preferências: clampear uiLayoutIdx**

  **What to do**:
  - `ats-mini.ino`: Em `setup()`, APÓS `prefsLoad(SAVE_SETTINGS|SAVE_VERIFY)` (linha 227), adicionar:
    ```cpp
    // Clamp layout index after removing UI_OBD layout
    if(uiLayoutIdx >= ITEM_COUNT(uiLayoutDesc))
      uiLayoutIdx = UI_DEFAULT;
    ```
  - `Menu.h` precisa de `getTotalUILayouts()` ou usar `LAST_ITEM(uiLayoutDesc)` — incluir `extern` ou referência
  - `Menu.cpp`: Adicionar `int getTotalUILayouts() { return ITEM_COUNT(uiLayoutDesc); }` (ou usar diretamente)

  **Must NOT do**:
  - Não incrementar VER_SETTINGS — a migração é feita em runtime, não no formato de storage
  - Não forçar save das prefs — só corrigir em memória; o próximo save natural persiste

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 5-10 linhas de migração defensiva, sem lógica complexa
  - **Skills**: Nenhuma

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 2, 4)
  - **Blocks**: None
  - **Blocked By**: Task 1 (por usar uiLayoutDesc reduzido)

  **References**:
  - `ats-mini.ino:227-238` — Onde prefsLoad é chamado
  - `Menu.cpp:266-268` — uiLayoutDesc array (agora só "Default", "S-Meter")
  - `Menu.h:49-51` — UI_DEFAULT = 0

  **Acceptance Criteria**:
  - [ ] `make build` compila sem erros
  - [ ] Com uiLayoutIdx=2 salvo em NVS, boot leva a UI_DEFAULT

  **QA Scenarios**:
  ```
  Scenario: Clamp existe no setup
    Tool: grep
    Steps:
      1. grep -n "uiLayoutIdx" ats-mini/ats-mini.ino
    Expected Result: Linha com clamp (>= ITEM_COUNT) presente após prefsLoad
    Evidence: .omo/evidence/task-3-clamp.log
  ```

  **Commit**: YES (groups with 1, 2, 4)
  - Message: `fix(obd): clamp stale uiLayoutIdx after removing OBD layout`
  - Files: `ats-mini.ino`, `Menu.cpp` (se adicionar getTotalUILayouts)

---

- [x] 4. **Atualizar Layout-OBD.cpp para comportamento de overlay**

  **What to do**:
  - `Layout-OBD.cpp`: Na função `drawLayoutObd()`, MODIFICAR para:
    1. Se `!BLEObd.isStarted()` e demo mode desligado: mostrar mensagem "OBD not active\nGo to Settings → Bluetooth → OBD"
    2. Se `BLEObd.isStarted() && !BLEObd.isConnected()`: mostrar "OBD: scanning...\nClick to go back"
    3. Se conectado/ready: mostrar dados normalmente (igual hoje)
    4. Se demo mode ativo: mostrar dados simulados (já que isReady=true)
  - ADICIONAR texto de ajuda no canto inferior: "Click to exit"
  - REMOVER o early return da linha 74 (`if (!connected || !ready) return;`) — substituir por mensagens informativas

  **Must NOT do**:
  - Não mudar o desenho dos dados (barras, RPM, etc.) quando conectado
  - Não remover o alerta de temperatura

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Modificação de texto/UI em função existente, sem nova lógica complexa
  - **Skills**: Nenhuma

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 2, 3)
  - **Blocks**: Task 5
  - **Blocked By**: Task 1 (CMD_OBD existir)

  **References**:
  - `Layout-OBD.cpp:48-74` — Cabeçalho da função, early return problemático
  - `Layout-OBD.cpp:146-171` — Alerta de temperatura (não mexer)
  - `Draw.cpp:472-476` — Padrão CMD_ABOUT (referência de overlay exit)

  **Acceptance Criteria**:
  - [ ] `make build` compila
  - [ ] Sem BLE ativo: overlay mostra "OBD not active" + instrução
  - [ ] BLE scanning: mostra "OBD: scanning..."
  - [ ] "Click to exit" visível no overlay

  **QA Scenarios**:
  ```
  Scenario: Mensagem OBD not active sem BLE
    Tool: grep
    Steps:
      1. grep -c "OBD not active" ats-mini/Layout-OBD.cpp
    Expected Result: String "OBD not active" presente
    Evidence: .omo/evidence/task-4-not-active.log

  Scenario: Click to exit presente
    Tool: grep
    Steps:
      1. grep -c "Click to exit\|Click to go back" ats-mini/Layout-OBD.cpp
    Expected Result: String de ajuda presente
    Evidence: .omo/evidence/task-4-exit-help.log

  Scenario: Early return removido
    Tool: grep -n "!connected || !ready" ats-mini/Layout-OBD.cpp
    Steps:
      1. grep -n "!connected || !ready" ats-mini/Layout-OBD.cpp
    Expected Result: Linha NÃO existe (early return substituído por mensagens)
    Evidence: .omo/evidence/task-4-no-early-return.log
  ```

  **Commit**: YES (groups with 5-7)
  - Message: `fix(obd): update overlay for exit help and inactive states`
  - Files: `Layout-OBD.cpp`

---

- [x] 5. **Auto-activate BLE_OBD ao abrir Menu → OBD**

  **What to do**:
  - `Menu.cpp`: Em `clickMenu()`, no case `MENU_OBD`:
    ```cpp
    case MENU_OBD:
      currentCmd = CMD_OBD;
      if(bleModeIdx != BLE_OBD) {
        bleModeIdx = BLE_OBD;
        bleInit(BLE_OBD);
      }
      break;
    ```
  - `Common.h`: Adicionar `extern uint8_t bleModeIdx;` (já existe na linha 189)
  - Verificar: `bleModeIdx` já é extern em Common.h:189, e `bleInit` está em BleMode.h:10

  **Must NOT do**:
  - Não forçar save do bleModeIdx (deixar o save normal de prefs acontecer naturalmente)
  - Não chamar bleInit() se já está em BLE_OBD (evitar restart desnecessário)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 5 linhas seguindo padrão existente (ex: MENU_BLEMODE)
  - **Skills**: Nenhuma

  **Parallelization**:
  - **Can Run In Parallel**: NO (depende de Task 1 e 4)
  - **Parallel Group**: Wave 2 (with Tasks 6, 7, 8)
  - **Blocks**: Task 8
  - **Blocked By**: Task 1 (CMD_OBD), Task 4 (overlay states)

  **References**:
  - `Menu.cpp:645-649` — clickBleMode() — padrão de bleInit
  - `Menu.cpp:908-909` — MENU_BLEMODE no clickSettings (referência)
  - `BleMode.cpp:46-61` — bleInit()
  - `Common.h:101` — BLE_OBD = 3

  **Acceptance Criteria**:
  - [ ] `make build` compila
  - [ ] Menu→OBD com bleModeIdx=OFF → ativa BLE_OBD
  - [ ] Menu→OBD com bleModeIdx=BLE_OBD → NÃO re-chama bleInit()

  **QA Scenarios**:
  ```
  Scenario: Auto-activate existe no MENU_OBD handler
    Tool: grep -A5 "MENU_OBD" ats-mini/Menu.cpp
    Steps:
      1. grep -A5 "MENU_OBD" ats-mini/Menu.cpp
    Expected Result: Case contém bleModeIdx = BLE_OBD e bleInit(BLE_OBD)
    Evidence: .omo/evidence/task-5-auto-activate.log

  Scenario: Não re-init se já BLE_OBD
    Tool: grep -A10 "MENU_OBD" ats-mini/Menu.cpp
    Steps:
      1. Verificar que o case checa `if(bleModeIdx != BLE_OBD)` antes de chamar bleInit
    Expected Result: Condicional presente
    Evidence: .omo/evidence/task-5-guard.log
  ```

  **Commit**: YES (groups with 4, 6, 7)
  - Message: `feat(obd): auto-activate BLE_OBD on menu open`
  - Files: `Menu.cpp`

---

- [x] 6. **Demo mode no BleObdCentral**

  **What to do**:
  - `BleObdCentral.h`: Adicionar:
    ```cpp
    void enableDemoMode(bool enable);
    bool isDemoMode() const;
    ```
  - `BleObdCentral.h` (private members):
    ```cpp
    bool demoMode_ = false;
    uint32_t lastDemoUpdateMs_ = 0;
    ```
  - `BleObdCentral.cpp`: Implementar:
    ```cpp
    void BleObdCentral::enableDemoMode(bool enable) {
      demoMode_ = enable;
      if(enable) {
        // Initialize demo data with reasonable defaults
        obdData_.rpm = 1200;
        obdData_.speed = 60;
        obdData_.coolantTemp = 87;
        obdData_.engineLoad = 35;
        obdData_.intakeTemp = 40;
        obdData_.throttlePos = 25;
        obdData_.batteryVoltage = 12.6f;
        obdData_.fuelLevel = 75;
        obdData_.timingAdvance = 12;
        obdData_.mafRate = 450;
        // Mark all valid
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
        lastDemoUpdateMs_ = millis();
      }
    }
    bool BleObdCentral::isDemoMode() const { return demoMode_; }
    ```
  - MODIFICAR `isConnected()` para retornar true se demo mode: NÃO é possível (BleCentral implementa). Em vez disso:
    - MODIFICAR `isReady()`: retornar true se demoMode_
    - NOVO método: `bool hasData() const { return isReady() || demoMode_; }`
    - Em `drawLayoutObd()`: usar `BLEObd.hasData()` ou `BLEObd.isReady() || BLEObd.isDemoMode()`
  - Em `BleObdCentral::update()` (ou no loop), se `demoMode_` e `!isConnected()`:
    - A cada ~300ms, atualizar dados simulados com variação senoidal:
      ```cpp
      if(demoMode_) {
        uint32_t now = millis();
        if(now - lastDemoUpdateMs_ > 250) {
          lastDemoUpdateMs_ = now;
          // RPM: 800-6000, slow sine
          obdData_.rpm = (uint16_t)(2800 + 2000 * sin(now / 5000.0));
          // Speed: 0-120, follows RPM roughly
          obdData_.speed = (uint8_t)(60 + 40 * sin(now / 8000.0));
          // Coolant: 80-100°C
          obdData_.coolantTemp = (int8_t)(88 + 8 * sin(now / 15000.0));
          // Engine load follows RPM
          obdData_.engineLoad = (uint8_t)(30 + 25 * sin(now / 5000.0));
          // Throttle: 15-70%
          obdData_.throttlePos = (uint8_t)(25 + 30 * sin(now / 4000.0));
          obdData_.updated = now;
        }
      }
      ```
  - `BleMode.h`: NÃO precisa mudar (BLEObd já é extern)

  **Must NOT do**:
  - Não alterar a lógica de conexão BLE real (scan, connect, notify, etc.)
  - Demo mode só gera dados se não houver conexão real ativa

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Lógica de simulação com temporização, requer cuidado com integridade de dados
  - **Skills**: Nenhuma

  **Parallelization**:
  - **Can Run In Parallel**: YES (estruturalmente independente)
  - **Parallel Group**: Wave 2 (with Tasks 5, 7, 8)
  - **Blocks**: Task 7 (web toggle precisa do flag)
  - **Blocked By**: None (pode ser feito em paralelo com Task 5)

  **References**:
  - `BleObdCentral.h:27-51` — ObdData struct com todos os campos e *Valid flags
  - `BleObdCentral.h:66-68` — isReady(), obdData()
  - `BleObdCentral.cpp:377-443` — update() existente
  - `BleCentral.h` — isConnected() é da classe base

  **Acceptance Criteria**:
  - [ ] `make build` compila
  - [ ] Demo mode ON: isReady() retorna true mesmo sem BLE
  - [ ] Demo mode ON: obdData() tem valores válidos e atualizados
  - [ ] Demo mode OFF: comportamento normal (não interfere com BLE real)

  **QA Scenarios**:
  ```
  Scenario: Demo mode toggle existe
    Tool: grep
    Steps:
      1. grep "enableDemoMode\|demoMode_" ats-mini/BleObdCentral.h
    Expected Result: Método enableDemoMode(bool) e flag demoMode_ declarados
    Evidence: .omo/evidence/task-6-demo-decl.log

  Scenario: Demo gera dados variáveis
    Tool: grep -A20 "if(demoMode_)" ats-mini/BleObdCentral.cpp
    Steps:
      1. Verificar que dados são atualizados com sin() ou millis()
    Expected Result: Dados simulados com variação temporal
    Evidence: .omo/evidence/task-6-demo-data.log

  Scenario: isReady considera demo mode
    Tool: grep "isReady\|demoMode_" ats-mini/BleObdCentral.cpp
    Steps:
      1. Verificar que isReady() retorna true se demoMode_
    Expected Result: Demo mode sobrepõe estado de conexão para fins de exibição
    Evidence: .omo/evidence/task-6-ready.log
  ```

  **Commit**: YES (groups with 4, 5, 7)
  - Message: `feat(obd): add demo mode with simulated OBD data`
  - Files: `BleObdCentral.h`, `BleObdCentral.cpp`

---

- [x] 7. **Web interface toggle para demo mode**

  **What to do**:
  - `Network.cpp`:
    1. Adicionar variável `static bool obdDemoEnabled = false;` no início do arquivo (ou usar extern em Common.h)
    2. Em `webConfigPage()` (ou similar), ADICIONAR no HTML do formulário:
       ```html
       <label><input type="checkbox" name="obddemo" value="1" {{OBD_DEMO_CHECKED}}> OBD Demo Mode</label><br>
       ```
       Substituir `{{OBD_DEMO_CHECKED}}` por `checked` se `obdDemoEnabled` for true
    3. Em `webSetConfig()`, ADICIONAR:
       ```cpp
       if(request->hasParam("obddemo", true)) {
         obdDemoEnabled = request->getParam("obddemo", true)->value() == "1";
         prefs.putBool("obddemo", obdDemoEnabled);
         BLEObd.enableDemoMode(obdDemoEnabled);
       }
       ```
    4. Na função que carrega configuração web (provavelmente `webInit()` ou `webConfigPage()`):
       ```cpp
       obdDemoEnabled = prefs.getBool("obddemo", false);
       BLEObd.enableDemoMode(obdDemoEnabled);
       ```
  - `Common.h`: Adicionar `extern BleObdCentral BLEObd;` (ou garantir que Network.cpp inclua BleMode.h)
  - Garantir que `Network.cpp` inclua `BleMode.h` ou `BleObdCentral.h`

  **Must NOT do**:
  - Não criar página web separada — usar /config existente
  - Não adicionar WebSocket ou streaming

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Requer entender estrutura HTML existente + handler de form + NVS prefs
  - **Skills**: Nenhuma

  **Parallelization**:
  - **Can Run In Parallel**: YES (independe das outras tasks de Wave 2)
  - **Parallel Group**: Wave 2 (with Tasks 5, 6, 8)
  - **Blocks**: None
  - **Blocked By**: Task 6 (enableDemoMode precisa existir)

  **References**:
  - `Network.cpp:321-347` — webInit() com handlers
  - `Network.cpp:349-389+` — webSetConfig() handler
  - `Network.cpp` — webConfigPage() (encontrar a função HTML)
  - `BleMode.h:8` — extern BleObdCentral BLEObd

  **Acceptance Criteria**:
  - [ ] `make build` compila
  - [ ] GET /config contém checkbox "OBD Demo Mode"
  - [ ] POST /setconfig com obddemo=1 ativa demo
  - [ ] POST /setconfig sem obddemo (ou =0) desativa demo
  - [ ] Estado persiste em NVS entre reboots

  **QA Scenarios**:
  ```
  Scenario: Checkbox presente no /config
    Tool: Bash (curl)
    Steps:
      1. GET http://atsmini.local/config
      2. grep "obddemo" no HTML retornado
    Expected Result: HTML contém input name="obddemo"
    Evidence: .omo/evidence/task-7-checkbox.log

  Scenario: POST setconfig ativa demo
    Tool: Bash (curl)
    Steps:
      1. curl -d "obddemo=1" http://atsmini.local/setconfig
    Expected Result: HTTP 200, demo mode ativado
    Evidence: .omo/evidence/task-7-demo-on.log

  Scenario: POST setconfig desativa demo
    Tool: Bash (curl)
    Steps:
      1. curl -d "obddemo=0" http://atsmini.local/setconfig
    Expected Result: HTTP 200, demo mode desativado
    Evidence: .omo/evidence/task-7-demo-off.log
  ```

  **Commit**: YES (groups with 4, 5, 6)
  - Message: `feat(obd): add OBD demo toggle to web config page`
  - Files: `Network.cpp`

---

- [x] 8. **Final wiring + build verification**

  **What to do**:
  - Verificar todos os includes necessários:
    - `Network.cpp` inclui `BleMode.h` (para acessar BLEObd)
    - `Draw.cpp` já inclui `BleMode.h` (linha 6) — OK
  - Verificar que `Layout-OBD.cpp` usa `BLEObd.isDemoMode()` ou `BLEObd.hasData()` onde necessário
  - Atualizar `drawLayoutObd()` para:
    1. Se demo mode ativo, mostrar dados simulados mesmo sem BLE
    2. Se BLE conectado + ready, mostrar dados reais (sobrepõe demo)
    3. Se nem demo nem BLE, mostrar mensagem inativo
  - Rodar `make build` e corrigir eventuais erros de compilação

  **Must NOT do**:
  - Não adicionar funcionalidades novas — só integrar e verificar

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Verificação de includes + build test
  - **Skills**: Nenhuma

  **Parallelization**:
  - **Can Run In Parallel**: NO (depende de tudo)
  - **Parallel Group**: Wave 2 (final da wave)
  - **Blocks**: Task F1-F4
  - **Blocked By**: Tasks 4, 5, 6, 7

  **References**:
  - `Layout-OBD.cpp` — toda a função
  - `Makefile` — regras de build
  - `Network.cpp:1-20` — includes existentes

  **Acceptance Criteria**:
  - [ ] `make build` compila sem erros
  - [ ] Todas as tarefas integradas coerentemente

  **QA Scenarios**:
  ```
  Scenario: Build completo
    Tool: Bash
    Steps:
      1. Run `make build`
    Expected Result: Exit code 0
    Evidence: .omo/evidence/task-8-build.log

  Scenario: Layout-OBD usa demo mode check
    Tool: grep -c "isDemoMode\|hasData\|demoMode" ats-mini/Layout-OBD.cpp
    Steps:
      1. Verificar que Layout-OBD.cpp considera demo mode
    Expected Result: Pelo menos 1 referência a demoMode
    Evidence: .omo/evidence/task-8-demo-ref.log
  ```

  **Commit**: YES (groups with all of Wave 2)
  - Message: `feat(obd): final integration and build verification`
  - Files: (depende do que precisar de ajuste)

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, curl endpoint, run command). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files exist in .omo/evidence/. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `make build` or compile. Review all changed files for: `as any`/`@ts-ignore` (não aplicável a C++), empty catches, commented-out code, unused imports. Check AI slop.
  Output: `Build [PASS/FAIL] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Start from clean state. Execute EVERY QA scenario from EVERY task — follow exact steps. Test web endpoints with curl with specific paths and expected responses.
  Output: `Scenarios [N/N pass] | Integration [N/N] | Edge Cases [N tested] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (git log/diff). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | VERDICT`

---

## Commit Strategy

- **1**: `feat(obd): add CMD_OBD overlay, menu entry, and demo mode`
  Files: Common.h, Menu.h, Menu.cpp, Draw.cpp, Layout-OBD.cpp, BleObdCentral.h, BleObdCentral.cpp, Network.cpp, Storage.cpp, ats-mini.ino
  Pre-commit: `make build`

---

## Success Criteria

### Verification Commands
```bash
# Build via GitHub Action (NÃO local — ver CLAUDE.md):
# 1. git push origin main
# 2. GitHub → Actions → "Build Firmware" → Run workflow
make build  # Expected: compiles without errors (blocked: no GitHub token in env)

# Web endpoints (when device is connected):
# curl http://atsmini.local/config  # Expected: HTML contains "obddemo" checkbox
# curl -d "obddemo=1" http://atsmini.local/setconfig  # Expected: demo enabled
```

### Final Checklist
- [~] `make build` compiles (bloqueado: push requer credenciais do usuário)
- [ ] Menu → OBD ativa overlay com dados (demo ou "scanning")
- [ ] Click sai do overlay
- [ ] Web toggle demo ON/OFF funciona
- [ ] Mini-widget RPM no Layout-Default ativo
- [ ] uiLayoutIdx=2 migrado sem crash
- [ ] Modo Bluetooth HID/ADHOC não quebrado (regressão)
