#!/usr/bin/env sh
set -eu

# Usage:
#   cd /path/to/MeshCore
#   /path/to/MC_rak4631_eth_gw/scripts/apply_overlay.sh /path/to/MC_rak4631_eth_gw
#
# This copies the new Ethernet TCP Companion transport into a MeshCore checkout.
# You still need to apply the small manual edits documented in patches/.

OVERLAY_REPO="${1:-}"
if [ -z "$OVERLAY_REPO" ]; then
  echo "usage: $0 /path/to/MC_rak4631_eth_gw" >&2
  exit 2
fi

if [ ! -f platformio.ini ] || [ ! -d examples/companion_radio ]; then
  echo "error: run this from the root of a MeshCore checkout" >&2
  exit 1
fi

mkdir -p src/helpers/nrf52
cp "$OVERLAY_REPO/meshcore_overlay/src/helpers/nrf52/EthernetSerialInterface.h" src/helpers/nrf52/EthernetSerialInterface.h
cp "$OVERLAY_REPO/meshcore_overlay/src/helpers/nrf52/EthernetSerialInterface.cpp" src/helpers/nrf52/EthernetSerialInterface.cpp

echo "copied EthernetSerialInterface into MeshCore."
echo "next: apply edits from $OVERLAY_REPO/patches/0001-add-rak4631-ethernet-companion-target.patch"
echo "then build: pio run -e RAK_4631_companion_radio_eth"
