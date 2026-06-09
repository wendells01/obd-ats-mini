# Draft: OBD Bugs + Demo Mode Improvements

## Requirements (confirmed)

### Bug 1: OBD indicator "OBD" no topo da tela
- O texto "OBD" vermelho na posição (190, 0) aparece "grande e bugado" ao sair do menu
- Suspeita: vazamento de font (`Orbitron_Light_24` do `setFreeFont` contamina o `drawString` com font 1)
- Causa raiz: o estado da font livre não é resetado antes de `drawObdIndicator`

### Bug 2: Speed "60km/h" acima dos dados no layout OBD
- O valor da velocidade (desenhado em `drawObdPanel` na posição y=22) está muito alto
- O texto grande `Orbitron_Light_24` (font 4) na posição (242, 22) fica sobreposto/colado na linha divisória (y=18)
- Precisa ser reposicionado mais para baixo

### Bug 3: RPM text atrás do ponteiro no gauge
- O texto "1200" (ou valor de RPM) no centro do gauge fica "atrás do ponteiro"
- O `fillCircle` do pivô (cx, cy, 4) aparece entre os caracteres do RPM porque o fundo do texto é transparente
- O texto RPM é desenhado DEPOIS do ponteiro (correto), mas o pivô aparece através dos gaps dos caracteres

### Feature 4: Demo mode melhorado
- Quer um ciclo automático: idle → aceleração → cruising → desaceleração → SHIFT → repetir
- RPM precisa chegar a 6000+ para testar o SHIFT indicator
- Dados do painel (speed, coolant, engine load, throttle) devem variar realisticamente junto com RPM

### Feature 5: Melhorias visuais no layout OBD
- Texto "Click to exit" sobrepõe com a última linha do painel (Intake Temp em y=138)
- Gauge pode descer demais (centro y=95, raio 65 → até y=160)

## Technical Decisions
- Usar `spr.setFreeFont(NULL)` para resetar font antes de `drawObdIndicator`
- Para RPM no centro: usar `spr.setTextPadding(spr.textWidth("8888", 4))` para fundo sólido OU reposicionar o texto mais para cima
- Para speed: ajustar posição y no panel (mover de y para y+4 ou mais)
- Demo mode: implementar máquina de estados no `update()` do `BleObdCentral`

## Scope Boundaries
- INCLUDE: OBD indicator fix, OBD layout fixes (speed position, RPM text, help text overlap), improved demo mode
- EXCLUDE: Other layouts (Default, S-Meter) unless affected by OBD indicator, theme changes, BLE connection improvements
