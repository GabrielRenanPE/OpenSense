#!/usr/bin/env bash
set -euo pipefail
if (( EUID != 0 )); then
  echo 'Execute: sudo bash uninstall.sh' >&2
  exit 1
fi
rm -f /usr/local/bin/opensense /usr/local/libexec/opensense-helper /usr/local/share/applications/opensense.desktop /usr/local/share/icons/hicolor/scalable/apps/opensense.svg
rm -f /usr/local/libexec/opensense-fan-helper /usr/share/polkit-1/actions/org.opensense.cooling.policy
printf '%s\n' 'OpenSense removido. Modos de GPU, limites de carga, drivers e preferências não foram alterados.'
