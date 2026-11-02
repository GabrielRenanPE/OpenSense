# Histórico

## Não lançado — revisão de segurança

- Conversões numéricas de valores lidos do sysfs (PWM, RPM, capacidade da bateria) agora são limitadas antes do `int`, evitando comportamento indefinido se um driver devolver um valor absurdo.
- O aplicativo chama o `pkexec` por caminho absoluto verificado (root, sem escrita de grupo/outros) em vez de usar o primeiro `pkexec` do `PATH`.
- Mensagens de erro vindas do auxiliar são exibidas como texto puro, nunca interpretadas como HTML.
- O binário `opensense-fan-helper` é compilado sem o código de GPU e de bateria; o teste de escopo passou a verificar isso.
- Binários com canário de pilha, RELRO completo (`-z relro -z now`) e PIE, quando o compilador suporta.
- `drivers/install-driver.sh` recusa uma árvore de fontes com links simbólicos antes de copiá-la para `/usr/src`.
- Novos testes: valores absurdos de PWM/Linuwu e ausência de código de GPU/bateria no auxiliar de resfriamento.

## 0.6.0 — beta

- Animações vetoriais de ventoinhas com folha, robô, foguete e pessoa.
- Estados animados de carga e proteção da bateria.
- Aba Sobre com história do projeto e perfis do criador.
- Pausa de animações em abas ocultas e com redução de movimento.
- Empacotamento público: documentação, GPL-2.0-or-later, créditos, templates e CI. Sem alterações no código operacional do driver.

## 0.5.0

- Correção de sobreposição ao remover opacidade da pilha de páginas.
- Novo layout dark e preservação da seleção do slider durante atualização.
- Auxiliar dedicado e autorização Polkit para resfriamento em sessão local ativa.

## 0.4.1

- Driver ANV15-51 empacotado com DKMS 0.1.0.
- Instalação, status e remoção do módulo específico.

## 0.4.0

- Interface dark com identidade shuriken e movimentos locais.

As versões anteriores fazem parte do desenvolvimento; não se afirma que existam tags públicas correspondentes.
