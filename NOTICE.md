# Créditos e licenciamento

## OpenSense

Copyright (C) 2026 Gabriel Renan Silva Martins Guimarães e colaboradores, na medida dos direitos detidos em suas contribuições.

O aplicativo, os auxiliares, scripts próprios, testes, documentação e recursos próprios deste pacote são disponibilizados sob GNU General Public License, versão 2 ou qualquer versão posterior (GPL-2.0-or-later). O texto da GPLv2 está em LICENSE. Este aviso não substitui avisos de terceiros nem reivindica autoria exclusiva sobre componentes derivados.

## Driver acer-wmi

- Base: driver Acer WMI Laptop Extras, do Linux. Avisos originais de Carlos Corbacho e E. M. Smith preservados no fonte.
- Referência da adaptação ANV15-51: patch de Justin Vargas, [mensagem original](https://lists.openwall.net/linux-kernel/2026/09/20/304).
- A base fornecida pelo criador foi informada como Linux v7.3. Não há commit upstream exato verificado neste pacote; não se afirma uma proveniência mais precisa. O SHA-256 da base está em drivers/acer-wmi-opensense-0.1.0/ORIGEM.txt.
- Licença: GPL-2.0-or-later, preservada no cabeçalho. COPYING, ORIGEM.txt e o patch acompanham os fontes.
- A adaptação local e o empacotamento DKMS não tornam o OpenSense autor de todo o driver.

## Dependências externas

Qt, Polkit, EnvyControl, DKMS e ferramentas do sistema não são redistribuídos neste pacote de fontes. São projetos independentes, com suas próprias licenças e autores. O aplicativo é ligado dinamicamente ao Qt. A integração com EnvyControl ocorre por execução do programa externo instalado no sistema.

Acer, Nitro e NitroSense são nomes de seus respectivos titulares. Não há afiliação, endosso ou suporte oficial da Acer para este projeto.
