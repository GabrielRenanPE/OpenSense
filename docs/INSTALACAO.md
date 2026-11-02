# Instalação no CachyOS / Arch

## 1. Preparar as dependências

```bash
sudo pacman -Syu --needed base-devel cmake qt6-base qt6-wayland polkit
```

Se a atualização trocar o kernel, reinicie no novo kernel antes de instalar um driver para ele. QtTest está incluído no qt6-base do Arch. GPU híbrida/integrada requer EnvyControl instalado separadamente; não é baixado pelo OpenSense. Controle da bateria pode exigir um driver separado, não incluído aqui.

## 2. Instalar o aplicativo

Extraia o pacote ou clone o repositório e abra o terminal na pasta que contém CMakeLists.txt e install.sh.

```bash
bash install.sh --app-only
opensense
```

Execute como usuário comum. O script compila, testa e solicita sudo no final para instalar. Não execute a interface com sudo. Feche versões anteriores, inclusive pela bandeja, antes de atualizar. Essa operação não precisa reiniciar o notebook e não modifica o driver.

Arquivos instalados: executável em /usr/local/bin; auxiliares em /usr/local/libexec; atalho e ícone em /usr/local/share; política em /usr/share/polkit-1/actions/org.opensense.cooling.policy.

## 3. Driver opcional

Confira [COMPATIBILIDADE.md](COMPATIBILIDADE.md) e [guia do driver](../drivers/README.md). Não use em outro modelo nem retire a restrição do kernel para forçar instalação.

```bash
bash install.sh --driver-only
```

Quando concluir sem erros, selecione Auto, reinicie e execute `bash drivers/status.sh`. Não reinicie se a regeneração de initramfs falhar.

`bash install.sh` sem opções instala o aplicativo e tenta instalar o driver no modelo Acer Nitro ANV15-51. Use --app-only quando o driver já funciona.

## 4. Remover

Aplicativo, auxiliares e política, mantendo driver e configurações de hardware:

```bash
sudo bash uninstall.sh
```

Driver: selecione Auto primeiro, então execute:

```bash
sudo bash drivers/remove-driver.sh
```

Reinicie somente após a remoção e regeneração do initramfs concluírem sem erro. Para remover ambos, remova primeiro o driver e depois o aplicativo. O desinstalador do aplicativo atende ao prefixo padrão /usr/local.
