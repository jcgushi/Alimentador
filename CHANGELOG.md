# Changelog

Todas as mudanças abaixo foram feitas no workspace local (não houve criação de repositório Git ou pull request remoto).

## [Unreleased]

### Corrigido
- script.js: Corrigida a função `renderTimers` — antes usava variáveis incorretas (parâmetro `timers` que sobrescrevia o array e referência a `t` inexistente). Agora exibe corretamente horário e passos.

### Melhorias — Frontend (data/script.js)
- Validação do formulário de criação de timers — valida `steps` e aplica limite (constante `MAX_STEPS_UI` para alinhar com limite do firmware).
- Tratamento de erros nas chamadas `fetch` (add/edit/delete/test) — agora verifica `response.ok` e exibe mensagens de erro em `alert()` quando houver falha.
- Confirmação ao excluir timer e validações ao editar timer/testar dose.
- Mover handler da navegação (nav links) para dentro do `DOMContentLoaded` garantindo que o DOM exista antes do uso.

### Melhorias — Firmware (src/main.cpp)
- `runMotorDose` agora mantido como wrapper legado; adicionada nova função `runMotorDoseSafe(uint32_t steps)` que:
  - valida parâmetros (steps > 0 e limite máximo)
  - evita sobreposição de comandos quando o motor está ocupado (usa `motores.stepstogo(0)`)
  - protege a enfileiramento da execução com `noInterrupts()`/`interrupts()` para reduzir condições de corrida com a ISR do driver
  - retorna booleano indicando sucesso/fracasso do agendamento
- Logging de doses melhorado:
  - `report.txt` passa a registrar eventos com marcação `INICIO` e `CONCLUIDO` (funções `logDoseStart` e `logDoseComplete`), permitindo diferenciar pedidos agendados de execuções concluídas.
- Monitoramento de conclusão: `checkTimersAndTrigger()` agora verifica se a dose em andamento terminou e registra `CONCLUIDO` no log.
- `handleTestTimer` e disparos via timer foram atualizados para usar `runMotorDoseSafe` e tratar falhas (HTTP 500 para falhas no agendamento quando motor estiver ocupado).

### Notas / recomendações
- As mudanças evitam condições de corrida com a ISR do driver (`motbepled`) e melhoram robustez e a experiência de usuário quando o motor está ocupado.
- Recomenda-se considerar: enfileiramento de doses (ao invés de recusar), expor um endpoint `/motor/status` para checar estado do motor, e usar verbos HTTP apropriados (POST) para operações que modificam estado.

---

Se quiser, posso agora:
- inicializar um repositório git local, criar branch e commitar essas alterações, e/ou
- gerar os comandos (git + GitHub CLI) para você rodar localmente e abrir um PR remoto.

Diga qual opção prefere e eu preparo os passos (ou executo se autorizar a criar/usar remotes).