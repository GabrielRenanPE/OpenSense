#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if (( EUID == 0 )); then
  echo 'Execute bash install.sh como usuário normal. A instalação solicitará sudo no final.' >&2
  exit 1
fi
mode=${1:-default}
case "$mode" in
  default|--app-only|--driver-only) ;;
  *) echo 'Uso: bash install.sh [--app-only|--driver-only]' >&2; exit 1 ;;
esac
if [[ "$mode" == --driver-only ]]; then
  sudo bash drivers/install-driver.sh
  exit
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
sudo cmake --install build
if [[ "$mode" != --app-only ]] &&
   [[ $(cat /sys/class/dmi/id/sys_vendor 2>/dev/null) == Acer ]] &&
   [[ $(cat /sys/class/dmi/id/product_name 2>/dev/null) == 'Nitro ANV15-51' ]]; then
  sudo bash drivers/install-driver.sh
else
  echo 'Aplicativo instalado; driver específico não instalado nesta execução.'
fi
printf '%s\n' 'OpenSense instalado. Abra pelo menu de aplicativos ou execute opensense.'
