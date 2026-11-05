#!/bin/bash
set -eu
qa90_acq=/workspace/qa-linux-22/C/KAN-90/version-comparison/acquisition
export HOME="$qa90_acq/flatpak-home"
export XDG_DATA_HOME="$HOME/.local/share" XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_RUNTIME_DIR="$qa90_acq/runtime"
export LD_LIBRARY_PATH=/workspace/qa-linux-22/C/KAN-90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu
export PATH=/workspace/qa-linux-22/C/KAN-90/sysroot/usr/bin:$PATH
mkdir -p "$HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
exec "$@"
