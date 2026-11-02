# Driver persistente — Acer Nitro ANV15-51

## Compatibilidade

- Hardware: fabricante DMI `Acer`, produto DMI exato `Nitro ANV15-51`, x86_64.
- Série de kernel habilitada para compilação: **7.3.x**. O funcionamento foi confirmado pelo usuário em **7.3.0-4-cachyos**, BIOS anteriormente informada V1.26. Outras revisões 7.3.x ainda dependem de compilação e teste.
- O kernel LTS 6.18 e outras séries são ignorados por BUILD_EXCLUSIVE_KERNEL. Eles continuam com o driver fornecido pela distribuição; a leitura de RPM/manual pode não existir neles.
- Não é um driver universal para Acer Nitro/Predator. Não force DMI ou parâmetros para outro modelo.
- Driver `acer-wmi-opensense/0.1.0`; módulo resultante `acer_wmi`. O código operacional é o mesmo do experimento que funcionou, com MODULE_VERSION adicionado para identificação pelo DKMS.

## Instalação no CachyOS / Arch

Os headers devem corresponder ao kernel que está em execução. O usuário já possui DKMS e headers 7.3.0-4. Se faltarem ferramentas, os nomes usuais de pacotes são `base-devel`, `dkms`, `linux-cachyos-headers`, `clang`, `llvm` e `lld`. Não faça atualização parcial dos headers; se atualizar kernel e headers juntos, reinicie no kernel correspondente antes de instalar.

Feche o OpenSense depois de selecionar **Auto**. Na raiz do projeto, como usuário normal:

```bash
bash install.sh
```

O aplicativo será compilado/testado/instalado e, somente no ANV15-51, o instalador configurará o driver como padrão. A senha é solicitada por sudo. Uma falha no driver é retornada como erro; a instalação do aplicativo que já terminou permanece.

Somente o driver (para quem já tem OpenSense):

```bash
bash install.sh --driver-only
```

Somente o aplicativo, sem tocar em drivers:

```bash
bash install.sh --app-only
```

O instalador não descarrega o driver em uso. Se terminar com sucesso, reinicie depois de selecionar Auto. Depois do boot:

```bash
bash drivers/status.sh
```

O esperado é um caminho `.../updates/dkms/acer-wmi.ko...`, versão carregada `0.1.0`, parâmetro `predator_v4` em `N`, arquivos pwm1/pwm2 e leituras RPM. `modinfo` sozinho consulta o módulo no disco; o status também lê a versão do módulo realmente carregado em /sys.

## O que a instalação modifica

1. Copia fontes para `/usr/src/acer-wmi-opensense-0.1.0`.
2. Registra, compila e instala via DKMS para o kernel em execução. Não usa --force nem sobrescreve diretamente o .ko do pacote linux-cachyos.
3. Confere se a resolução do módulo seleciona o arquivo DKMS esperado.
4. Escreve apenas dois arquivos próprios:
   - `/etc/modprobe.d/zzzz-opensense-anv15-51.conf`: seleciona a identificação específica (`predator_v4=0 force_series=0 force_caps=-1`).
   - `/etc/modules-load.d/opensense-anv15-51.conf`: carrega `acer_wmi` no boot por meio da infraestrutura normal, com dependências.
5. Regenera initramfs usando o gerador encontrado, incluindo variantes Limine. A ordem segue o script update-initramfs do CachyOS.

Configurações anteriores não são apagadas. O arquivo com prefixo zzzz normalmente vem depois delas. Configurações personalizadas ordenadas depois dele, blacklists, regras `install acer_wmi`, ou flags de boot podem interferir; o status pós-boot é necessário. Parâmetros acer_wmi explícitos na linha de boot fazem o instalador parar antes de instalar.

A configuração modprobe.d vale também para kernels sem esta adaptação. No LTS, isso pode remover as leituras obtidas anteriormente com predator_v4=1; não significa que o driver PWM foi instalado no LTS.

O módulo acer_wmi_battery, a configuração da GPU e o limite de bateria não são alterados. O modo manual não é persistido: tornar o driver padrão não significa fixar uma velocidade no boot.

## Atualizações de kernel

AUTOINSTALL=yes permite ao mecanismo DKMS da distribuição recompilar nas atualizações da série 7.3.x, com os headers instalados. DKMS não garante compatibilidade com mudanças de API. Confira a saída da atualização e `dkms status`; se falhar, mantenha um kernel funcional disponível e investigue antes de depender do controle manual.

Uma atualização para 7.4 ou outra série é deliberadamente ignorada até revisar o fonte. Não remova a restrição apenas para silenciar o aviso. Quando o kernel da distribuição oferecer suporte nativo completo ao ANV15-51, prefira remover este módulo externo e validar o suporte nativo.

## Falhas e recuperação

- Falha de build: as configurações de boot ainda não foram escritas. Consulte `/var/lib/dkms/acer-wmi-opensense/0.1.0/build/make.log`. O registro e as fontes podem permanecer para diagnóstico; use o desinstalador para remover o registro.
- Falha de instalação/verificação: o módulo atual continua carregado. Não assuma que o próximo boot foi configurado; use o desinstalador ou corrija o erro e execute novamente.
- Falha de initramfs: o instalador retorna erro e **não** informa sucesso. Não reinicie até corrigir e repetir a instalação ou a remoção. Não há rollback automático de imagens de boot parcialmente geradas.
- Lockdown ativo: esta versão do instalador interrompe antes de instalar. Ela não desativa Secure Boot nem matrícula chaves. Instalação com assinatura e matrícula de chave exige um fluxo específico, ainda não incluído.
- Unknown symbol: use modprobe, não insmod avulso; ele resolve dependências, inclusive sparse_keymap.

## Remover somente o driver

Selecione Auto no OpenSense e execute na raiz do projeto:

```bash
sudo bash drivers/remove-driver.sh
```

Isso remove todas as instalações DKMS desta versão e somente os dois arquivos de configuração gerenciados, recalcula dependências e regenera initramfs. Depois reinicie. O driver do pacote do kernel volta a ser selecionado; parâmetros anteriores voltam a valer. A fonte em /usr/src é preservada para auditoria. O aplicativo e a proteção de bateria permanecem instalados.

`sudo bash uninstall.sh` remove apenas o aplicativo, não o driver. Para remover ambos, execute primeiro o desinstalador do driver e depois o do aplicativo.

## Compartilhar / licenças

Distribua este projeto com a pasta completa `drivers/`, incluindo os fontes, Makefile, Kbuild, dkms.conf, patch, ORIGEM.txt, COPYING e estes documentos. Não distribua apenas um .ko: ele depende do kernel de destino.

O arquivo acer-wmi.c conserva os avisos originais e o identificador SPDX **GPL-2.0-or-later**. COPYING contém a GPLv2; a cláusula “or later” permanece no fonte. Os novos scripts de instalação e o empacotamento do driver também são disponibilizados sob GPL-2.0-or-later. Nesta edição pública, o aplicativo e os arquivos próprios também são oferecidos sob GPL-2.0-or-later; consulte ../NOTICE.md e ../LICENSE.

A implementação base é do projeto Linux, com os autores preservados. A adaptação de hardware deriva do patch de Justin Vargas abaixo; não apresente todo o driver como código novo do OpenSense. A adaptação para esta base mantém predator_v4=1 dentro da configuração específica, pois ela não tem os campos hwmon/platform_profile separados da referência. A identificação DMI usa correspondência exata.

Fontes verificáveis:
- https://lists.openwall.net/linux-kernel/2026/09/20/304
- https://man.archlinux.org/man/dkms.8.en
- https://man.archlinux.org/man/modprobe.d.5.en
- https://github.com/CachyOS/cachyos-hooks/blob/master/update-initramfs

## Validação desta entrega

O funcionamento físico do driver experimental foi confirmado pelo usuário antes deste empacotamento. A instalação DKMS, a assinatura e o boot desta versão precisam ser confirmados na máquina de destino; não foram executados no ambiente de preparação. Consulte VALIDACAO.md na raiz para as verificações locais.
