#!/usr/bin/env bash
set -u
printf 'Kernel: '; uname -r
printf 'Driver selecionado para o próximo carregamento: '; modinfo -n acer_wmi
printf 'Versão selecionada: '; modinfo -F version acer_wmi
printf 'Versão realmente carregada: '; cat /sys/module/acer_wmi/version 2>/dev/null || echo 'sem versão'
printf 'predator_v4 carregado: '; cat /sys/module/acer_wmi/parameters/predator_v4 2>/dev/null
command -v dkms >/dev/null && dkms status -m acer-wmi-opensense -v 0.1.0
for d in /sys/class/hwmon/hwmon*; do
  [[ $(cat "$d/name" 2>/dev/null) == acer ]] || continue
  printf '\nSensor: %s\n' "$d"
  for attr in pwm1 pwm1_enable pwm2 pwm2_enable fan1_input fan2_input; do
    printf '%s: ' "$attr"; cat "$d/$attr" 2>/dev/null || echo indisponível
  done
done
