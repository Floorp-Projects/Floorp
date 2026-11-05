#!/bin/bash
set -eu
qa90=/workspace/qa-linux-22/C/KAN-90
qaCompare="$qa90/version-comparison"
export HOME="$qaCompare/home"
export XDG_DATA_HOME="$HOME/.local/share" XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_RUNTIME_DIR="$qaCompare/runtime"
export LD_LIBRARY_PATH="$qa90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu"
export PATH="$qa90/sysroot/usr/bin:/workspace/.floorp-cloud/bin:$PATH"
export FLATPAK_DBUSPROXY="$qa90/sysroot/usr/bin/xdg-dbus-proxy"
exec "$@"
