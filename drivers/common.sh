#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
name=acer-wmi-opensense
version=0.1.0
source_dir=/usr/src/$name-$version
config=/etc/modprobe.d/zzzz-opensense-anv15-51.conf
boot_config=/etc/modules-load.d/opensense-anv15-51.conf
marker='# Managed by OpenSense acer-wmi-opensense 0.1.0'
fail() { echo "ERRO: $*" >&2; exit 1; }
require_root() { (( EUID == 0 )) || fail 'Execute com sudo.'; }
select_initramfs() {
  if command -v limine-mkinitcpio >/dev/null; then regen=(limine-mkinitcpio)
  elif command -v mkinitcpio >/dev/null; then regen=(mkinitcpio --allpresets)
  elif command -v limine-dracut >/dev/null; then regen=(limine-dracut)
  elif command -v dracut >/dev/null; then regen=(dracut --regenerate-all --force)
  elif [[ -x /usr/lib/booster/regenerate_images ]]; then regen=(/usr/lib/booster/regenerate_images)
  else fail 'Gerador de initramfs não identificado. Nenhuma instalação será iniciada.'; fi
}
check_owned() {
  local f=$1
  [[ ! -L "$f" ]] || fail "Link simbólico inesperado: $f"
  if [[ -e "$f" ]] && ! grep -Fxq "$marker" "$f"; then fail "Arquivo preexistente não gerenciado: $f"; fi
}
