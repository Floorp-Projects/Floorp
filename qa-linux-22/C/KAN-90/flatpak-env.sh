#!/bin/bash
set -eu
qa90=/workspace/qa-linux-22/C/KAN-90
export MOZ_APP_REMOTINGNAME=qa-kan90-floorp
export HOME="$qa90/home"
export XDG_DATA_HOME="$HOME/.local/share" XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_RUNTIME_DIR="$qa90/runtime"
export LD_LIBRARY_PATH="$qa90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu"
export PATH="$qa90/sysroot/usr/bin:/workspace/.floorp-cloud/bin:$PATH"
mkdir -p "$HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
exec "$@"
