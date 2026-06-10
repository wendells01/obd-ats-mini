# OBD Interface Redesign — BMW M3 Competition Style

## TL;DR

> **Quick Summary**: Reformular completamente a interface OBD com 2 telas premium estilo BMW M3 Competition, navegação por encoder (T1↔T2), SHIFT overlay universal, e customização de PIDs via WebUI.
>
> **Deliverables**:
> - Tela 1: Tacômetro circular grande + velocidade em destaque (BMW M3 G80 digital cluster style)
> - Tela 2: Grade de dados 6 linhas, PIDs customizáveis via WebUI on/off
> - Navegação: encoder gira T1↔T2, click sai do OBD
> - SHIFT overlay universal (tela cheia) em qualquer tela OBD
> - WebUI: `/api/obd/config` endpoint + checkboxes na página `/config`
> - Dead code cleanup: remover `UI_OBD` do layout selector
>
> **Estimated Effort**: Medium (10-14 tasks)
> **Parallel Execution**: YES — 4 waves
> **Critical Path**: Task 1 → Tasks 5,6,7 (parallel) → Task 9 → Build → QA

---

## Context

### Original Request
Usuário quer reformulação completa da interface OBD: telas premium estilo BMW M3 Competition, navegação por encoder entre telas, SHIFT overlay em qualquer tela, customização de PIDs via WebUI, e tudo otimizado.

### Interview Summary
**Key Discussions**:
- 2 telas: T1 = tacômetro+velocidade (BMW M3 G80 style), T2 = dados gerais 6 linhas
- Encoder gira T1↔T2 (wrap-around), click sai do OBD
- SHIFT overlay = tela cheia (atual, mantido idêntico) em QUALQUER tela OBD
- Customização WebUI: só liga/desliga PIDs na Tela 2 (sem drag-and-drop)
- Demo mode mantido
- Visual BMW M3 Competition digital cluster (fundo escuro, contraste alto, minimalista)
- Testes: CI build (3 variantes) + usuário flasha + feedback loop

**Research Findings**:
- `CMD_OBD` (0x3100) é overlay mode — não é menu/settings, excluído do timeout em `ats-mini.ino:939`
- Encoder em OBD mode cai em `doSideBar()` → retorna false → rotação ignorada
- `UI_OBD` (2) definido mas inacessível (`UI_LAYOUT_COUNT=2`) — dead code em Draw.cpp:519
- 10 PIDs coletados via BLE, 6 exibidos no dispositivo atualmente
- 3 fontes de dados: real (BLE ELM327), demo (simulação), override (WebUI)
- WebUI `/obd` com dashboard + override endpoints já existe
- Tela atual: gauge 130px esquerda + painel 145px direita (crammed)
- Tema TH.* usado extensivamente; SHIFT usa TFT_YELLOW/TFT_RED fixos

### Metis Review
**Identified Gaps** (addressed):
- **Encoder handler**: Criar `doObdNavigation()` — não usar `doSideBar()` para OBD. Chamar de um novo `case CMD_OBD:` no main loop.
- **SHIFT preservado**: Copiar código atual verbatim, overlaying sobre T1 ou T2
- **obdScreenIdx global**: 0 = T1, 1 = T2, sempre resetar pra T1 ao entrar no OBD
- **PID toggles só afetam display**: Todos os 10 PIDs continuam sendo polldos mesmo se invisíveis na T2
- **UI_OBD dead code**: Remover `#define UI_OBD` de Menu.h e `case UI_OBD:` de Draw.cpp
- **Edge cases**: "No PIDs selected" na T2, SHIFT não bloqueia click, entrada OBD sempre T1
- **NVS**: Bump VER_SETTINGS para adicionar chaves `obd_pid_*`

---

## Work Objectives

### Core Objective
Reformular a interface OBD do ATS Mini com 2 telas premium estilo BMW M3 Competition, navegação por encoder, SHIFT overlay universal, e customização WebUI de PIDs.

### Concrete Deliverables
- `Layout-OBD.cpp` reescrito: `drawObdScreenT1()` + `drawObdScreenT2()` + SHIFT overlay
- `Common.h`: novo `extern uint8_t obdScreenIdx` + `extern bool obdPidEnabled[]`
- `ats-mini.ino`: novo `case CMD_OBD:` no switch de rotação do encoder
- `Menu.h`: remover `#define UI_OBD`
- `Draw.cpp`: remover `case UI_OBD:` fallback
- `Network.cpp`: novo endpoint `POST /api/obd/config` + PID checkboxes no `/config`
- `BleObdCentral.h/cpp`: sem alterações (data pipeline preservado)

### Definition of Done
- [ ] CI build passa nas 3 variantes (ospi, qspi, lilygo-t-embed)
- [ ] Encoder gira T1↔T2 no hardware
- [ ] SHIFT overlay aparece em T1 e T2 quando RPM ≥ 6000
- [ ] WebUI toggle PID → PID some/reaparece na T2 (após reboot)
- [ ] Click sai do OBD de volta ao rádio
- [ ] `UI_OBD` dead code removido
- [ ] T1 mostra tacômetro BMW-style + velocidade grande

### Must Have
- Navegação T1↔T2 via encoder (CW avança, CCW retrocede, wrap)
- SHIFT overlay (cópia verbatim do código atual) em T1 E T2
- PID toggles persistem via NVS
- Todos 10 PIDs continuam sendo polldos independente da visibilidade na T2
- Entrada no OBD sempre começa na T1
- Click durante SHIFT overlay ainda sai do OBD
- Demo mode continua funcionando

### Must NOT Have (Guardrails)
- Sem terceira tela OBD
- Sem drag-and-drop/reordenação de PIDs
- Sem alterações no `ObdData` struct ou no polling engine BLE
- Sem novas ações de botão no encoder em modo OBD (click = sair, só)
- Sem alterações na estrutura do main loop (`ats-mini.ino`)
- Sem alterações no Bluetooth/WiFi stack
- SHIFT overlay NÃO pode ser modificado visualmente (mantido idêntico)

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: NO (Arduino, sem test framework)
- **Automated tests**: Nenhum — CI build + hardware test + feedback loop
- **Evidence**: Captura de saída serial, curl contra WebUI endpoints, logs de build

### QA Policy
Cada task inclui cenários QA executáveis via curl (WebUI override) + logs seriais.
- **WebUI**: curl GET/POST para `/api/obd`, `/api/obd/set`, `/api/obd/config`
- **Build**: `gh workflow run "Build Firmware"` + `gh run watch`
- **Serial debug**: Usar `Serial.printf` para dump de `obdScreenIdx`, RPM, SHIFT state

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Foundation — START IMMEDIATELY, max parallel):
├── Task 1: Common.h — Add obdScreenIdx, PID toggle NVS keys, bump VER_SETTINGS [quick]
├── Task 2: Menu.h/Draw.cpp — Remove UI_OBD dead code [quick]
├── Task 3: Network.cpp — Add POST /api/obd/config endpoint + PID checkboxes [unspecified]
└── Task 4: Layout-OBD.h — Declare new functions (drawObdScreenT1, T2, doObdNavigation) [quick]

Wave 2 (Core Screens — depends on Task 1, max parallel):
├── Task 5: Layout-OBD.cpp — Create drawObdScreenT1() (BMW M3 tachometer + speed) [visual-engineering]
├── Task 6: Layout-OBD.cpp — Create drawObdScreenT2() (6-row PID grid, WebUI-filtered) [visual-engineering]
├── Task 7: Layout-OBD.cpp — Add doObdNavigation() + obdScreenIdx logic + SHIFT overlay [deep]
└── Task 8: ats-mini.ino — Add case CMD_OBD in encoder rotation switch [quick]

Wave 3 (Integration — depends on Wave 2):
├── Task 9: Layout-OBD.cpp — Refactor drawLayoutObd() to dispatch T1/T2 + SHIFT [deep]
├── Task 10: Network.cpp — Update /api/obd JSON to include PID toggle state [quick]
└── Task 11: Build + Commit + Push + CI trigger [git]

Wave FINAL (Parallel reviews, then user okay):
├── F1: Plan compliance audit (oracle)
├── F2: Code quality review (unspecified-high)
├── F3: Real QA via override API + serial debug (unspecified-high)
└── F4: Scope fidelity check (deep)
→ Present results → Get explicit user OK

Critical Path: Task 1 → Tasks 5,6,7 (parallel) → Task 9 → Task 11 → F1-F4 → user OK
```

### Dependency Matrix
- **1**: - → 5,6,7
- **2**: - → (independent)
- **3**: - → 10
- **4**: - → 5,6,7
- **5**: 1,4 → 9
- **6**: 1,4 → 9
- **7**: 1,4 → 8,9
- **8**: 7 → 9
- **9**: 5,6,7,8 → 11
- **10**: 3 → 11
- **11**: 9,10 → Final
- **F1-F4**: 11 → user OK

### Agent Dispatch Summary
- Wave 1: 4 tasks → `quick` × 3, `unspecified-high` × 1
- Wave 2: 4 tasks → `visual-engineering` × 2, `deep` × 1, `quick` × 1
- Wave 3: 3 tasks → `deep` × 1, `quick` × 1, `git` × 1
- Final: 4 tasks → `oracle`, `unspecified-high` × 2, `deep`

---

## TODOs

- [x] 1. **Common.h — Add obdScreenIdx, PID toggle NVS keys, bump VER_SETTINGS**

  **What to do**:
  - Add `extern uint8_t obdScreenIdx` (0 = T1, 1 = T2)
  - Add `extern bool obdPidEnabled[10]` — array de 10 bools para cada PID
  - Add `#define OBD_PID_COUNT 10` e enum/mapeamento PID index → name
  - Bump `VER_SETTINGS` (incrementar em 1) para forçar reset de prefs na primeira execução
  - Add NVS load/save para `obdPidEnabled[]` no bloco de prefs (seguir padrão `prefsRequestSave(SAVE_ALL)`)
  - Default: todos exceto RPM e Speed habilitados (RPM+Speed já na T1)

  **Must NOT do**:
  - Não modificar `ObdData` struct
  - Não mudar polling intervals

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Alterações localizadas em 1 arquivo, mudanças mecânicas
  - **Skills**: Nenhum (mecânico)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 2, 3, 4)
  - **Blocks**: Tasks 5, 6, 7
  - **Blocked By**: None

  **References**:
  - `ats-mini/Common.h:VER_APP`, `VER_SETTINGS` — versões atuais para incrementar
  - `ats-mini/Common.h:currentCmd`, `uiLayoutIdx` — padrão de globals extern
  - `ats-mini/Common.h:prefsRequestSave()`, `prefsLoad()` — padrão de save/load NVS
  - `ats-mini/BleObdCentral.h:ObdData` — struct existente (NÃO modificar)

  **Acceptance Criteria**:
  - [ ] `grep "extern uint8_t obdScreenIdx" Common.h` → encontrado
  - [ ] `grep "extern bool obdPidEnabled\[10\]" Common.h` → encontrado
  - [ ] `grep "VER_SETTINGS" Common.h` → valor incrementado
  - [ ] Código compila: `gh workflow run "Build Firmware"` → success

  **QA Scenarios**:
  ```
  Scenario: Verify obdScreenIdx and PID toggle symbols are declared
    Tool: Bash (grep)
    Steps:
      1. grep -n "obdScreenIdx\|obdPidEnabled\|OBD_PID_COUNT" ats-mini/Common.h
    Expected Result: All 3 symbols found with correct types (uint8_t, bool[10], #define)
    Evidence: .omo/evidence/task-1-symbols.txt
  ```

  **Commit**: NO (groups with 2-4)

---

- [x] 2. **Menu.h/Draw.cpp — Remove UI_OBD dead code**

  **What to do**:
  - `Menu.h`: Remover `#define UI_OBD 2` (comentar ou deletar a linha)
  - `Draw.cpp`: Remover o `case UI_OBD:` fallback no `switch(uiLayoutIdx)`:
    ```cpp
    // ANTES:
    case UI_OBD:
      drawLayoutObd();
      break;
    // DEPOIS: (removido)
    ```
  - Verificar se `UI_OBD` é referenciado em outros lugares (grep) — se sim, limpar também

  **Must NOT do**:
  - Não alterar `UI_LAYOUT_COUNT` (deve continuar 2)
  - Não alterar `UI_DEFAULT` ou `UI_SMETER`

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Remoção mecânica de código morto, 1-2 arquivos

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 3, 4)
  - **Blocks**: None (purge de código morto)
  - **Blocked By**: None

  **References**:
  - `ats-mini/Menu.h` — local da definição `#define UI_OBD 2`
  - `ats-mini/Draw.cpp` — local do `case UI_OBD:` no switch
  - `ats-mini/Layout-OBD.cpp` — verificar se referencias `UI_OBD`

  **Acceptance Criteria**:
  - [ ] `grep "UI_OBD" Menu.h` → não encontrado (ou comentado)
  - [ ] `grep "case UI_OBD" Draw.cpp` → não encontrado
  - [ ] Código compila sem warnings

  **QA Scenarios**:
  ```
  Scenario: Verify UI_OBD dead code removed
    Tool: Bash (grep)
    Steps:
      1. grep -rn "UI_OBD" ats-mini/Menu.h ats-mini/Draw.cpp
    Expected Result: No lines match (or only comment explaining removal)
    Evidence: .omo/evidence/task-2-no-obd-layout.txt
  ```

  **Commit**: NO (groups with 1, 3, 4)

---

- [x] 3. **Network.cpp — Add POST /api/obd/config endpoint + PID checkboxes in /config page**

  **What to do**:
  - Add `POST /api/obd/config` handler que recebe `pid_<NAME>=0|1` para cada PID
  - Salva configuração em NVS (usar namespace `"obd"` prefs, ou adicionar ao `"settings"`)
  - Adicionar checkboxes na página `/config` (HTML) para cada PID, pré-selecionadas conforme estado atual
  - Os 10 PIDs: `rpm`, `speed`, `coolantTemp`, `engineLoad`, `intakeTemp`, `mafRate`, `throttlePos`, `timingAdvance`, `fuelLevel`, `batteryVoltage`
  - Seguir padrão existente do `/config` (Network.cpp:580-650 área do obddemo)
  - Carregar configuração no boot (em `webServerInit()` ou similar)
  - Atualizar `obdPidEnabled[]` quando receber POST

  **Must NOT do**:
  - Não modificar a estrutura da página `/obd` dashboard
  - Não adicionar drag-and-drop ou reordenação

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Requer entender estrutura HTTP handler + HTML + NVS persistence
  - **Skills**: Nenhum específico

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 2, 4)
  - **Blocks**: Task 10
  - **Blocked By**: None

  **References**:
  - `ats-mini/Network.cpp:obdDemoEnabled` handler (~linha 608) — padrão de checkbox POST em /config
  - `ats-mini/Network.cpp:webServerInit()` (~linha 332) — carregamento de prefs
  - `ats-mini/Network.cpp:handleObdSet()` (~linha 503) — padrão de handler POST /api/obd/*
  - `ats-mini/Network.cpp:webObdDashboardPage()` (~linha 967) — estrutura HTML com JS

  **Acceptance Criteria**:
  - [ ] `grep "/api/obd/config" Network.cpp` → encontrado
  - [ ] `grep "pid_coolantTemp\|pid_speed" Network.cpp` → checkboxes no /config
  - [ ] `curl -X POST http://ats-mini.local/api/obd/config -d "pid_coolant=0"` → 200 OK

  **QA Scenarios**:
  ```
  Scenario: Toggle PID off via WebUI
    Tool: Bash (curl)
    Steps:
      1. curl -s -X POST http://ats-mini.local/api/obd/config -d "pid_coolantTemp=0"
      2. curl -s http://ats-mini.local/api/obd | jq '.pidEnabled.coolantTemp'
    Expected Result: false (0)
    Evidence: .omo/evidence/task-3-pid-toggle.txt

  Scenario: Verify configuration page has checkboxes
    Tool: Bash (curl)
    Steps:
      1. curl -s http://ats-mini.local/config | grep -c "pid_"
    Expected Result: > 0 (checkboxes found in HTML)
    Evidence: .omo/evidence/task-3-config-page.txt
  ```

  **Commit**: NO (groups with 1, 2, 4)

---

- [x] 4. **Layout-OBD.h (or Draw.h) — Declare new screen functions**

  **What to do**:
  - Se existir `Draw.h`, adicionar declarações:
    ```cpp
    void drawObdScreenT1();
    void drawObdScreenT2();
    void doObdNavigation(int16_t enc);
    ```
  - Se não existir, declarar no topo de `Layout-OBD.cpp` como funções estáticas ou num header
  - Manter `drawLayoutObd()` como função de dispatch (já declarada)

  **Must NOT do**:
  - Não duplicar funções existentes

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Adição mecânica de declarações

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 2, 3)
  - **Blocks**: Tasks 5, 6, 7
  - **Blocked By**: None

  **References**:
  - `ats-mini/Draw.h` — declarações de funções de desenho existentes

  **Acceptance Criteria**:
  - [ ] Funções declaradas (grep encontra)
  - [ ] Compila sem "implicit declaration" warnings

  **Commit**: NO (groups with 1-3)

- [x] 5. **Layout-OBD.cpp — Create drawObdScreenT1() (BMW M3 tachometer + speed)**

  **What to do**:
  - Criar função `drawObdScreenT1()` que desenha a Tela 1 (tacômetro + velocidade)
  - **Design BMW M3 Competition G80 digital cluster:**
    - Fundo escuro (TH.bg)
    - Tacômetro circular grande (~140px diâmetro, centralizado)
    - Arco de 270° igual ao atual (135°→405°), mesmas zonas de cor (verde/amarelo/vermelho)
    - Tick marks a cada 500 RPM (mais refinado que atual 1000)
    - Agulha mais fina e elegante (triângulo alongado, cor TH.freq_text)
    - **Velocidade** exibida em destaque: número grande (Orbitron_Light_24, textSize(2) ≈48pt)
      - Posicionada DENTRO do arco do tacômetro ou abaixo do centro
      - Unidade "km/h" em fonte pequena ao lado
    - Pequeno indicador de gear/RPM numérico no canto (opcional)
    - Status "OBD: connected/demo/override" na barra superior (igual atual)
  - Obter dados via `BLEObd.obdData()` (const ref)
  - Se dados inválidos (não connected/demo/override), mostrar "No data" centralizado

  **Visual inspiration** (BMW M3 G80):
  - Fundo preto escuro (quase preto)
  - Tacômetro circular com luz de fundo sutil
  - Velocidade digital grande dentro do círculo
  - Marcadores de RPM brancos, zona vermelha no final
  - Design limpo, sem ruído visual, alta legibilidade

  **Must NOT do**:
  - Não adicionar animações complexas que consomem CPU
  - Não usar sprites extras (manter TFT_eSprite único `spr`)
  - Não modificar o sistema de temas TH.*

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: UI/UX design de painel digital premium + geometria de gauge
  - **Skills**: Nenhum específico (design visual + C++ embarcado)

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 6, 7, 8)
  - **Blocks**: Task 9
  - **Blocked By**: Task 1, Task 4

  **References**:
  - `ats-mini/Layout-OBD.cpp:drawObdGauge()` (linhas 201-302) — referência de geometria de arco/tick/needle
  - `ats-mini/Layout-OBD.cpp:drawObdPanel()` linhas de speed (fontes, posição) — referência de estilo
  - `ats-mini/BleObdCentral.h:ObdData` — `.rpm`, `.speed`
  - `ats-mini/Common.h:TH.*` — cores do tema atual
  - `ats-mini/Menu.h:SHIFT_RPM_LIMIT`, `OBD_MAX_RPM`

  **Acceptance Criteria**:
  - [ ] Função `drawObdScreenT1()` existe e compila
  - [ ] Tacômetro circular com arco 270° (verde/amarelo/vermelho)
  - [ ] Velocidade exibida como número grande dentro do arco
  - [ ] Agulha de RPM funcional
  - [ ] Compila nas 3 variantes

  **QA Scenarios**:
  ```
  Scenario: T1 renders with demo data
    Tool: Bash (curl + verify via serial)
    Preconditions: Demo mode enabled, OBD mode active
    Steps:
      1. Enable demo via WebUI /config (obddemo=1)
      2. Enter OBD mode on device
      3. Check serial output: "OBD T1: rpm=XXXX, speed=XXX"
    Expected Result: Tela 1 aparece com tacômetro e velocidade
    Evidence: .omo/evidence/task-5-t1-render.txt

  Scenario: T1 shows "No data" when disconnected
    Preconditions: BLE off, demo off
    Steps:
      1. Verify serial output: "OBD: no data"
    Expected Result: "No data" message centered on screen
  ```

  **Commit**: NO (groups with 6, 7, 8)

---

- [x] 6. **Layout-OBD.cpp — Create drawObdScreenT2() (6-row PID grid, WebUI-filtered)**

  **What to do**:
  - Criar função `drawObdScreenT2()` que desenha a Tela 2 (dados gerais)
  - **Layout:**
    - 6 linhas de dados, fontes grandes (~22px cada linha, ocupando toda altura: 170-20=150px, 150/6=25px por linha)
    - Cada linha: indicador (6px dot) + label (nome do PID) + valor + unidade
    - Layout em grade 1 coluna × 6 linhas (full width = 320px)
    - Usar `obdPidEnabled[]` para filtrar quais PIDs aparecem
    - Se um PID está desligado, pular a linha (linhas compactam para cima)
    - Se todos PIDs desligados, mostrar "No PIDs selected" centralizado
    - Status dot: verde (normal), amarelo (atenção), vermelho (crítico) — mesmos thresholds do painel atual
  - **Dados a exibir** (padrão, ordenados): coolantTemp, engineLoad, batteryVoltage, throttlePos, intakeTemp, fuelLevel
    - Obs: RPM e Speed NÃO aparecem por padrão na T2 (já estão na T1)
  - **BMW M3 style:**
    - Labels em fonte pequena (font 2, ~14px), cor TH.text_muted
    - Valores em fonte maior (font 4 ou Orbitron_Light_24 reduzido), cor TH.freq_text
    - Unidades em fonte pequena ao lado do valor
    - Mini barra de progresso horizontal (opcional, para Load e Throttle)
  - Obter dados via `BLEObd.obdData()` (const ref) + `obdPidEnabled[]`

  **Must NOT do**:
  - Não usar scroll (só 6 linhas, tudo visível)
  - Não adicionar gráficos ou gauges na T2
  - Não reordenar PIDs (ordem fixa)

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: Layout de dados premium, design de informação
  - **Skills**: Nenhum específico

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 5, 7, 8)
  - **Blocks**: Task 9
  - **Blocked By**: Task 1, Task 4

  **References**:
  - `ats-mini/Layout-OBD.cpp:drawObdPanel()` (linhas 11-196) — padrão de linha: dot + label + value + bar
  - `ats-mini/Layout-OBD.cpp` — thresholds de status (coolant >105°C red, etc.)
  - `ats-mini/BleObdCentral.h:ObdData` — todos os campos + *Valid flags
  - `ats-mini/Common.h:obdPidEnabled[]` — array de toggle (criado na Task 1)

  **Acceptance Criteria**:
  - [ ] Função `drawObdScreenT2()` existe e compila
  - [ ] 6 linhas de dados, filtradas por `obdPidEnabled[]`
  - [ ] Status dots funcionam (verde/amarelo/vermelho)
  - [ ] "No PIDs selected" quando todos desligados
  - [ ] Compila nas 3 variantes

  **QA Scenarios**:
  ```
  Scenario: T2 shows default 6 PIDs
    Tool: Bash (curl + serial)
    Preconditions: OBD mode active, demo on, T2 selected
    Steps:
      1. Gire encoder para T2
      2. Serial output: "OBD T2: showing 6 PIDs"
    Expected Result: 6 linhas visíveis, dados do demo
    Evidence: .omo/evidence/task-6-t2-default.txt

  Scenario: T2 hides PID when toggled off
    Preconditions: T2 visible
    Steps:
      1. curl -X POST /api/obd/config -d "pid_coolantTemp=0"
      2. Reboot device, enter OBD, T2
      3. Serial output: "OBD T2: showing 5 PIDs (coolantTemp hidden)"
    Expected Result: coolantTemp nao aparece, 5 linhas
    Evidence: .omo/evidence/task-6-t2-filtered.txt

  Scenario: T2 shows "No PIDs selected" when all off
    Steps:
      1. curl -X POST /api/obd/config -d "pid_coolantTemp=0&pid_engineLoad=0&..."
      2. Reboot
      3. Serial output: "OBD T2: no PIDs enabled"
    Expected Result: Mensagem "No PIDs selected" centralizada
  ```

  **Commit**: NO (groups with 5, 7, 8)

---

- [x] 7. **Layout-OBD.cpp — Add doObdNavigation() + obdScreenIdx dispatch + SHIFT overlay**

  **What to do**:
  - Criar função `doObdNavigation(int16_t enc)`:
    ```cpp
    void doObdNavigation(int16_t enc) {
      if (enc > 0) obdScreenIdx = (obdScreenIdx + 1) % 2;   // CW: avanca
      if (enc < 0) obdScreenIdx = (obdScreenIdx - 1 + 2) % 2; // CCW: retrocede
      needRedraw = true;
    }
    ```
  - Garantir que `obdScreenIdx` seja 0 (T1) ao entrar no modo OBD
  - **SHIFT overlay**: COPIAR VERBATIM o codigo atual (Layout-OBD.cpp linhas 347-367 atual) para dentro de `drawLayoutObd()`, APOS a renderizacao de T1 ou T2:
    ```cpp
    void drawLayoutObd() {
      if (condicoes de dados invalidos) { mostra hint; return; }
      switch (obdScreenIdx) {
        case 0: drawObdScreenT1(); break;
        case 1: drawObdScreenT2(); break;
      }
      // SHIFT overlay — copiado VERBATIM do codigo atual
      if (d.rpm >= SHIFT_RPM_LIMIT) {
        spr.fillRect(0, 19, 320, 151, TFT_YELLOW);
        spr.drawRect(0, 19, 320, 151, TFT_RED);
        // ... codigo blink exato igual ao atual ...
      }
    }
    ```
  - NAO MODIFICAR o codigo SHIFT — deve ser identico ao atual visualmente

  **Must NOT do**:
  - SHIFT visualmente identico ao atual (0 alteracoes)
  - SHIFT nao reseta `obdScreenIdx`
  - Click durante SHIFT deve SAIR do OBD

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Requer entender fluxo de navegacao + garantir SHIFT preservado
  - **Skills**: Nenhum especifico

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Tasks 5, 6, 8)
  - **Blocks**: Tasks 8, 9
  - **Blocked By**: Task 1, Task 4

  **References**:
  - `ats-mini/Layout-OBD.cpp:347-367` — codigo SHIFT atual (copiar verbatim)
  - `ats-mini/Common.h:obdScreenIdx` — global (criado Task 1)
  - `ats-mini/Common.h:needRedraw` — flag de redraw
  - `ats-mini/Menu.h:SHIFT_RPM_LIMIT` (6000)

  **Acceptance Criteria**:
  - [ ] `doObdNavigation()` existe e implementa wrap T1<T2
  - [ ] SHIFT overlay identico ao atual (diff = 0 linhas diferentes no bloco SHIFT)
  - [ ] SHIFT funciona em T1 e T2
  - [ ] SHIFT nao reseta `obdScreenIdx`
  - [ ] Click sai do OBD mesmo durante SHIFT

  **QA Scenarios**:
  ```
  Scenario: Navigation cycles T1→T2→T1
    Tool: curl + serial
    Preconditions: OBD mode active
    Steps:
      1. Serial: "obdScreenIdx=0 (T1)"
      2. Envie rotacao
      3. Serial: "obdScreenIdx=1 (T2)"
      4. Mais rotacao: "obdScreenIdx=0 (T1)"
    Expected Result: Ciclo T1→T2→T1

  Scenario: SHIFT overlay on T2
    Preconditions: OBD active, T2 visible
    Steps:
      1. curl -X POST /api/obd/set -d "mode=1&rpm=6500"
      2. Serial: "SHIFT active on T2"
    Expected Result: SHIFT overlay sobre T2
    Evidence: .omo/evidence/task-7-shift-t2.txt
  ```

  **Commit**: NO (groups with 5, 6, 8)

---

- [x] 8. **ats-mini.ino — Add case CMD_OBD in encoder rotation switch**

  **What to do**:
  - No main loop (`ats-mini.ino`), localizar o `switch(currentCmd)` que trata rotacao do encoder (em torno da linha 837)
  - Adicionar `case CMD_OBD:` que chama `doObdNavigation(encCount)`:
    ```cpp
    case CMD_OBD:
      if (encCount != 0) {
        doObdNavigation(encCount);
        needRedraw = true;
      }
      break;
    ```
  - Verificar que a entrada no modo OBD seta `obdScreenIdx = 0`
  - Garantir que timeout `ELAPSED_COMMAND` continua excluindo CMD_OBD

  **Must NOT do**:
  - Nao modificar estrutura do switch existente
  - Nao alterar handlers de click
  - Nao modificar logica de timeout

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Adicao de case simples em switch existente, 1 arquivo

  **Parallelization**:
  - **Can Run In Parallel**: NO (Task 8 precisa de `doObdNavigation()` da Task 7)
  - **Parallel Group**: Wave 2 (mas executa DEPOIS da Task 7)
  - **Blocks**: Task 9
  - **Blocked By**: Task 7

  **References**:
  - `ats-mini/ats-mini.ino:~837` — switch(currentCmd) para rotacao encoder
  - `ats-mini/ats-mini.ino:~939` — ELAPSED_COMMAND timeout
  - `ats-mini/ats-mini.ino:~906-932` — click handler chain

  **Acceptance Criteria**:
  - [ ] `grep "case CMD_OBD:" ats-mini.ino` → encontrado
  - [ ] `grep "doObdNavigation" ats-mini.ino` → encontrado
  - [ ] Compila sem warnings

  **QA Scenarios**:
  ```
  Scenario: Encoder navigation in OBD mode
    Tool: curl + serial
    Steps:
      1. Enter OBD mode
      2. Serial: "CMD_OBD active, obdScreenIdx=0"
      3. Simulate encoder rotation
      4. Serial: "CMD_OBD active, obdScreenIdx=1"
    Expected Result: Navegacao funciona, sem crashes
    Evidence: .omo/evidence/task-8-encoder-nav.txt
  ```

  **Commit**: NO (groups with 5, 6, 7)

---

- [x] 9. **Layout-OBD.cpp — Refactor drawLayoutObd() to dispatch T1/T2 + SHIFT overlay**

  **What to do**:
  - Reescrever `drawLayoutObd()` para:
    1. Verificar estado de conexao (igual ao atual: connected/demo/override?)
    2. Se nao conectado/demo/override, mostrar hint "No OBD data" + return
    3. Se conectado, mostrar barra de status "OBD: connected/demo/override"
    4. `switch (obdScreenIdx)` para chamar T1 ou T2
    5. APOS T1/T2 renderizado, verificar SHIFT e overlays por cima
  - Remover chamadas antigas de `drawObdGauge()` e `drawObdPanel()`
  - Garantir que a barra de status (linha 0-18) aparece em AMBAS as telas
  - Adicionar indicador de qual tela esta ativa (ex: "T1/T2" pequeno no canto)
  - Adicionar debug serial: `Serial.printf("OBD: screen=%d rpm=%d speed=%d\n", obdScreenIdx, d.rpm, d.speed)`

  **Must NOT do**:
  - SHIFT overlay deve ser identico ao atual
  - Nao mudar o comportamento de saida (click → CMD_NONE)

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Integracao de todas as partes, arquitetura do fluxo de rendering
  - **Skills**: Nenhum especifico

  **Parallelization**:
  - **Can Run In Parallel**: NO (precisa de T1, T2, encoder, SHIFT prontos)
  - **Parallel Group**: Wave 3
  - **Blocks**: Task 11
  - **Blocked By**: Tasks 5, 6, 7, 8

  **References**:
  - `ats-mini/Layout-OBD.cpp:307-368` — funcao drawLayoutObd() atual
  - `ats-mini/Draw.cpp:drawScreen()` — fluxo de chamada
  - `ats-mini/Common.h:obdScreenIdx`
  - `ats-mini/Menu.h:SHIFT_RPM_LIMIT`
  - **IMPORTANTE**: O codigo SHIFT overlay adicionado na Task 7 JA ESTA COMMITADO quando Task 9 executa. Use a versao COMMITADA do SHIFT (nao o original em Layout-OBD.cpp:347-367). Task 9 deve REAPROVEITAR o codigo SHIFT da Task 7, NAO copiar do original novamente. A Task 9 reescreve `drawLayoutObd()` mas DEVE manter o bloco SHIFT que Task 7 adicionou.

  **Acceptance Criteria**:
  - [ ] T1 e T2 aparecem conforme `obdScreenIdx`
  - [ ] SHIFT overlay funciona em T1 e T2
  - [ ] Status bar visivel em ambas telas
  - [ ] Debug serial mostra screenIdx, rpm, speed
  - [ ] Compila nas 3 variantes

  **QA Scenarios**:
  ```
  Scenario: Full flow T1→T2→SHIFT
    Tool: curl + serial
    Steps:
      1. Enter OBD mode (T1 appears)
      2. Rotate encoder CW (T2 appears)
      3. curl -X POST /api/obd/set -d "mode=1&rpm=6500&speed=120"
      4. SHIFT overlay on T2
      5. curl -X POST /api/obd/set -d "mode=1&rpm=3000"
      6. T2 reappears
      7. Click encoder → exit to radio
    Expected Result: Fluxo completo sem falhas
    Evidence: .omo/evidence/task-9-full-flow.txt
  ```

  **Commit**: NO (groups with 10, 11)

---

- [x] 10. **Network.cpp — Update /api/obd JSON to include PID toggle state**

  **What to do**:
  - No handler GET `/api/obd`, adicionar ao JSON:
    ```json
    {
      ...existing fields...,
      "obdScreenIdx": 0,
      "pidEnabled": {
        "rpm": true,
        "speed": true,
        "coolantTemp": true,
        "engineLoad": true,
        "intakeTemp": true,
        "mafRate": false,
        "throttlePos": true,
        "timingAdvance": false,
        "fuelLevel": true,
        "batteryVoltage": true
      }
    }
    ```
  - Usar `obdPidEnabled[]` para preencher os valores
  - Seguir o padrao de JSON existente (sprintf ou String concatenation)

  **Must NOT do**:
  - Nao modificar estrutura existente do JSON (so adicionar campos)
  - Nao quebrar compatibilidade com dashboard JS existente

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Adicao de campos a JSON existente, 1 arquivo
  - **Skills**: Nenhum especifico

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3 (with Task 9, 11)
  - **Blocks**: Task 11
  - **Blocked By**: Task 3

  **References**:
  - `ats-mini/Network.cpp:466-500` — handler GET /api/obd atual
  - `ats-mini/Common.h:obdPidEnabled[]` — array de toggle

  **Acceptance Criteria**:
  - [ ] `curl http://ats-mini.local/api/obd | jq '.pidEnabled'` → retorna objeto com 10 keys
  - [ ] Dashboard JS existente nao quebra (pagina /obd ainda carrega)

  **QA Scenarios**:
  ```
  Scenario: JSON includes pidEnabled
    Tool: Bash (curl + jq)
    Steps:
      1. curl -s http://ats-mini.local/api/obd | jq '.pidEnabled | length'
    Expected Result: 10
    Evidence: .omo/evidence/task-10-pid-enabled-json.txt
  ```

  **Commit**: NO (groups with 9, 11)

---

- [x] 11. **Build + Commit + Push + CI trigger**

  **What to do**:
  - Executar em ordem:
    1. `git add -A && git commit -m "feat: OBD redesign BMW M3 style - T1/T2 screens, encoder nav, PID WebUI config"`
    2. `git push obd main`
    3. `gh workflow run "Build Firmware" --ref main --repo wendells01/obd-ats-mini`
    4. `gh run watch <run-id> --repo wendells01/obd-ats-mini`
  - Verificar que todas as 3 variantes compilam
  - Se alguma falhar, corrigir e commitar novamente

  **Must NOT do**:
  - Nao fazer force push
  - Nao pular verificacao de build

  **Recommended Agent Profile**:
  - **Category**: `git` (via task dispatch)
    - Reason: Operacoes git + CI monitoring
  - **Skills**: Nenhum especifico

  **Parallelization**:
  - **Can Run In Parallel**: NO (precisa de tasks 9 e 10 completas)
  - **Parallel Group**: Wave 3 (sequential)
  - **Blocks**: Final Verification Wave
  - **Blocked By**: Tasks 9, 10

  **References**:
  - `.github/workflows/build.yml` — workflow CI
  - `AGENTS.md` — instrucoes de build

  **Acceptance Criteria**:
  - [ ] `git status` → clean
  - [ ] `gh run list --limit 1 --json conclusion --jq '.[0].conclusion'` → "success"
  - [ ] 3 variantes (ospi, qspi, lilygo) compilam

  **QA Scenarios**:
  ```
  Scenario: CI build passes all 3 variants
    Tool: Bash (gh CLI)
    Steps:
      1. gh run list --repo wendells01/obd-ats-mini --limit 1 --json conclusion --jq '.[0].conclusion'
    Expected Result: "success"
    Evidence: .omo/evidence/task-11-ci-success.txt
  ```

  **Commit**: YES — este e o commit final
  - Message: `feat: OBD redesign BMW M3 style - T1/T2 screens, encoder nav, PID WebUI config`
  - Files: `ats-mini/Common.h ats-mini/Menu.h ats-mini/Draw.cpp ats-mini/Network.cpp ats-mini/ats-mini.ino ats-mini/Layout-OBD.cpp`
  - Pre-commit: `git diff --stat` (verify only planned files changed)

---

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, curl endpoint). For each "Must NOT Have": search codebase for forbidden patterns. Check that SHIFT code was copied verbatim (no visual changes). Verify UI_OBD removed.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Check: dead code removal, no `// TODO` left, no commented-out blocks, no unused imports. Run `git diff --stat` to see if files changed are only those planned. Check for any accidental BLE/WiFi changes.
  Output: `Dead code [CLEAN/N issues] | Scope creep [CLEAN/N files] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Start from CI build artifacts. Flash to device (user step). Test: encoder navigation T1↔T2, SHIFT at 6000+, click exit, PID toggle persistence. Use WebUI override endpoint to set known values. Log serial output.
  Output: `Tests [N/N pass] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (git log/diff). Verify 1:1 — everything specified was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | VERDICT`

---

## Commit Strategy

- **1-4**: `refactor: add obdScreenIdx, remove UI_OBD dead code, add /api/obd/config endpoint`
- **5-8**: `feat: OBD T1 (BMW M3 tachometer) + T2 (PID grid) + encoder navigation`
- **9-11**: `feat: OBD redesign BMW M3 style — T1/T2 screens, encoder nav, SHIFT overlay, PID WebUI config`

---

## Success Criteria

### Pre-Execution
```bash
mkdir -p .omo/evidence
```

### Verification Commands
```bash
# Build
gh workflow run "Build Firmware" --ref main --repo wendells01/obd-ats-mini

# WebUI PID toggle
curl -X POST http://ats-mini.local/api/obd/config -d "pid_coolant=0"

# WebUI override RPM for SHIFT test
curl -X POST http://ats-mini.local/api/obd/set -d "mode=1&rpm=6500&speed=120"

# Check OBD state
curl http://ats-mini.local/api/obd | jq '{rpm, speed, overrideMode, obdScreenIdx}'
```

### Final Checklist
- [ ] Todas as 3 variantes compilam
- [ ] Encoder gira T1↔T2
- [ ] SHIFT overlay cobre T1 e T2 quando RPM ≥ 6000
- [ ] Click no encoder sai do OBD (mesmo durante SHIFT)
- [ ] PID toggle persiste após reboot
- [ ] UI_OBD removido (sem dead code)
- [ ] Demo mode continua funcionando
- [ ] Todos 10 PIDs ainda são polldos
