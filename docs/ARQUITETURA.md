# Arquitetura

## Aplicativo

src/main.cpp contém a janela Qt Widgets, navegação, leituras periódicas e coordenação de operações. src/system.hpp reúne detecção de interfaces, conversões, validação de comandos e operações de hardware. motion.hpp, design.hpp e status_art.hpp desenham a identidade e animações locais com Qt, sem navegador embutido.

## Separação de privilégios

A interface executa como usuário normal. Para operações de resfriamento, chama pkexec com o opensense-fan-helper. O mesmo fonte helper.cpp é compilado com OPENSENSE_FAN_ONLY=1, que restringe comandos a ventoinhas e perfis térmicos. GPU e bateria usam opensense-helper com a autenticação padrão.

A política Polkit associa um caminho exato ao auxiliar de resfriamento. Somente sessões locais ativas recebem a autorização padrão sem senha. Não há regra ampla para sudo ou para qualquer programa. A lista de comandos, limites e destinos de sysfs continua sendo validada no auxiliar.

## Driver

O módulo externo mantém o nome acer_wmi. DKMS compila para o kernel permitido e instala em updates/dkms; não distribua .ko de outra máquina. Os scripts verificam modelo e kernel e regeneram initramfs após instalar/remover. O aplicativo não implementa acesso bruto ao EC.

## Testes

- tests/core.cpp: conversões e regras do núcleo.
- tests/cooling-scope.cmake: rejeição de operações fora do escopo.
- tests/ui.cpp: navegação, renderização e estados das animações sem escritas no hardware.

A execução de testes não instala drivers. Consulte VALIDACAO.md para limites dos resultados.
