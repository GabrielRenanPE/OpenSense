# Validação 0.6.0

- Compilação C++17 / Qt 6.4.2 em Linux com avisos habilitados.
- Núcleo: 77 verificações de conversões, operações, limites e regras de hardware.
- Escopo do auxiliar: rejeição de GPU/bateria e entradas de ventoinha inválidas antes de acessar hardware.
- Interface offscreen: 36 trocas rápidas entre páginas, redimensionamento, ocultação/reabertura, uma única página visível e igualdade da imagem estabilizada com/sem redução de animações.
- Inspeção visual das telas Sobre, Bateria e Resfriamento renderizadas pelo Qt.
- Política Polkit e instalação em diretório temporário verificadas; nenhum arquivo instalado no sistema real.
- Sintaxe Bash verificada. Pasta drivers e system.hpp preservados em relação a 0.4.1.

Este ambiente não tem o notebook Acer, sessão Plasma/Wayland nem serviço Polkit ativo para verificar a autorização real. Não foram realizadas escritas de PWM, instalação DKMS, regeneração de initramfs ou reinicialização. O funcionamento final deve ser confirmado no notebook. As capturas de teste têm sensores ausentes porque este ambiente não dispõe do hardware.

Testes de animação: todos os quatro modos de ventoinha, carregando, descarregando, cheia, sem carregar, proteção ativada/desativada e estado desconhecido. Verifica movimento nos estados animados, imagem estática com redução de animações e temporizador parado quando oculto. O driver, os auxiliares e a política de autorização permanecem iguais à versão 0.5.0.

## Preparação para GitHub

Adicionados documentação, avisos de licença, templates e workflow. O código do aplicativo, dos auxiliares e do driver permanece igual ao pacote 0.6.0. O workflow do GitHub Actions ainda não foi executado no GitHub. Os testes locais anteriores continuam descritos acima; o empacotamento não amplia o suporte de hardware.

## Revisão de segurança (pós-0.6.0, ainda não lançada)

Executado em Ubuntu 24.04 com Qt 6.4.2, CMake e GCC: compilação limpa com `-Wall -Wextra`, `ctest` (núcleo com 82 verificações, escopo do auxiliar de resfriamento e interface offscreen) aprovado, análise com cppcheck e shellcheck, validação XML da política Polkit, instalação em diretório temporário (`DESTDIR`) sem tocar no sistema, e conferência com `readelf` de PIE, RELRO completo, `BIND_NOW` e canário de pilha nos três binários. O auxiliar de resfriamento foi executado como root no ambiente de teste (sem hardware Acer) apenas para confirmar a rejeição de operações fora do escopo.

Continuam sem verificação real: autorização Polkit em sessão Plasma/Wayland, escrita de PWM, DKMS, initramfs e o GitHub Actions. Isso precisa ser confirmado no notebook.
