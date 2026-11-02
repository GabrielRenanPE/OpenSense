#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
source ./common.sh
require_root
exec 9>/run/opensense-driver.lock
flock -n 9 || fail 'Outra operação do driver está em andamento.'
select_initramfs
check_owned "$config"
check_owned "$boot_config"
if [[ -n $(dkms status -m "$name" -v "$version") ]]; then
  dkms remove -m "$name" -v "$version" --all
fi
rm -f -- "$config" "$boot_config"
# Keep source files for inspection and reliable recovery; DKMS registration is removed.
for directory in /lib/modules/*; do
  [[ -d "$directory" ]] || continue
  depmod -a "${directory##*/}"
done
if ! "${regen[@]}"; then
  fail 'Falha ao regenerar initramfs. Não reinicie até corrigir e repetir este comando.'
fi
echo 'Driver DKMS removido. Fontes preservadas em /usr/src para auditoria.'
echo 'O driver em execução permanece até reiniciar. Selecione Auto no OpenSense antes de reiniciar.'
echo 'As configurações anteriores do acer_wmi voltam a valer; acer_wmi_battery não foi alterado.'
