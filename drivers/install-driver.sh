#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
source ./common.sh
require_root
[[ $(cat /sys/class/dmi/id/sys_vendor) == Acer && $(cat /sys/class/dmi/id/product_name) == 'Nitro ANV15-51' ]] || fail 'Driver restrito ao Acer Nitro ANV15-51.'
[[ $(uname -m) == x86_64 ]] || fail 'Arquitetura não suportada.'
kernel=$(uname -r)
[[ $kernel =~ ^7[.]3[.] ]] || fail "Kernel $kernel fora da série 7.3.x suportada por esta versão."
for tool in dkms make modprobe modinfo depmod flock; do command -v "$tool" >/dev/null || fail "Dependência ausente: $tool"; done
exec 9>/run/opensense-driver.lock
flock -n 9 || fail 'Outra instalação do driver está em andamento.'
[[ -f /lib/modules/$kernel/build/Module.symvers ]] || fail 'Instale os headers correspondentes ao kernel em uso.'
if grep -q '^CONFIG_CC_IS_CLANG=y$' /lib/modules/"$kernel"/build/.config; then
  for tool in clang ld.lld llvm-ar llvm-nm llvm-objcopy; do command -v "$tool" >/dev/null || fail "Ferramenta ausente: $tool"; done
fi
# Parameters on the kernel command line can override modprobe.d.
if grep -Eq 'acer[-_]wmi[.](predator_v4|force_series|force_caps)=' /proc/cmdline; then
  fail 'Existem parâmetros acer_wmi no boot. Remova esses parâmetros do boot antes de instalar.'
fi
if lsmod | grep -q '^linuwu'; then fail 'Linuwu-Sense está carregado; resolva o conflito antes de instalar.'; fi
installed_path=$(modinfo -n acer_wmi)
case "$installed_path" in
  */kernel/drivers/platform/x86/acer-wmi.ko*) ;;
  */updates/dkms/acer-wmi.ko*)
    dkms status -m "$name" -v "$version" | grep -q installed || fail 'Outro driver acer-wmi externo já está instalado.' ;;
  *) fail "Origem de acer-wmi não reconhecida: $installed_path" ;;
esac
if [[ -r /sys/kernel/security/lockdown ]] && grep -Eq '\[(integrity|confidentiality)\]' /sys/kernel/security/lockdown; then
  fail 'Kernel com lockdown: prepare assinatura e matrícula da chave DKMS antes de usar este instalador. Nenhuma proteção será desativada.'
fi
select_initramfs
check_owned "$config"
check_owned "$boot_config"
# O DKMS compila estas fontes como root: nenhum link simbólico pode apontar
# para arquivos de fora da árvore do driver.
[[ -d "$name-$version" ]] || fail "Pasta de fontes $name-$version não encontrada ao lado deste script."
[[ -z $(find "$name-$version" -type l -print -quit) ]] || fail 'A árvore de fontes do driver contém links simbólicos. Instalação interrompida.'
# Do not overwrite any source tree belonging to another installation/version.
if [[ -e "$source_dir" ]]; then
  diff -qr --exclude='*.o' "$name-$version" "$source_dir" >/dev/null || fail "Fontes diferentes já existem em $source_dir. Remova a versão anterior pelo desinstalador."
else
  cp -R --no-dereference -- "$name-$version" "$source_dir"
  chown -R root:root "$source_dir"
  chmod -R go-w "$source_dir"
fi
if [[ -z $(dkms status -m "$name" -v "$version") ]]; then dkms add -m "$name" -v "$version"; fi
# No boot settings change until build and install succeed.
dkms build -m "$name" -v "$version" -k "$kernel"
dkms install -m "$name" -v "$version" -k "$kernel"
depmod -a "$kernel"
selected=$(modinfo -k "$kernel" -n acer_wmi)
[[ "$selected" == */updates/dkms/acer-wmi.ko* ]] || fail "DKMS instalado, mas não selecionado: $selected. Não reinicie; consulte drivers/README.md."
[[ $(modinfo -k "$kernel" -F version acer_wmi) == "$version" ]] || fail 'Versão instalada inesperada.'
mkdir -p /etc/modprobe.d /etc/modules-load.d
printf '%s\n%s\n' "$marker" 'options acer_wmi predator_v4=0 force_series=0 force_caps=-1' > "$config"
printf '%s\n%s\n' "$marker" 'acer_wmi' > "$boot_config"
chmod 644 "$config" "$boot_config"
if ! "${regen[@]}"; then
  echo 'ERRO: atualização do initramfs falhou. Não reinicie ainda. Corrija o erro e repita o instalador ou execute sudo bash drivers/remove-driver.sh.' >&2
  exit 1
fi
dkms status -m "$name" -v "$version"
printf 'Driver padrão preparado: %s\n' "$selected"
echo 'O módulo em execução não foi recarregado. No OpenSense, selecione Auto antes de reiniciar.'
echo 'Depois de reiniciar: bash drivers/status.sh'
