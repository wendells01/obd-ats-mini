# ATS Mini - Project Memory

## Build

**NÃO compilar localmente.** Use a GitHub Action "Build Firmware":
1. Faça push das alterações para o GitHub
2. Vá em Actions → "Build Firmware" → "Run workflow"
3. Selecione a branch desejada e clique em "Run workflow"
4. A action compila para todas as variantes (ospi, qspi, lilygo-t-embed)
5. Monitore até o build terminar com sucesso

A action compila usando `arduino/compile-sketches@v1` com o profile `esp32s3-ospi` (e variantes).
O workflow está em `.github/workflows/build.yml`.

## Commits

- Use commits convencionais: `feat(scope):`, `fix(scope):`, `refactor(scope):`
- Verificar diff antes de commitar
- Não commitar secrets
