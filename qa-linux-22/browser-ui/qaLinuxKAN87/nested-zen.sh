#!/bin/bash
set -euo pipefail
base=/workspace/qa-linux-22/browser-ui
out="$base/qaLinuxKAN87"
export DISPLAY=:92
export XDG_RUNTIME_DIR="$out/runtime/wayland-zen"
export XDG_CACHE_HOME="$out/runtime/cache"
export XDG_CONFIG_HOME="$out/runtime/config"
export XDG_DATA_DIRS="$base/sysroot/usr/share:/usr/local/share:/usr/share"
export LD_LIBRARY_PATH="$base/sysroot/usr/lib/x86_64-linux-gnu:$base/sysroot/usr/lib/x86_64-linux-gnu/weston"
export QT_PLUGIN_PATH="$base/sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins:/usr/lib/x86_64-linux-gnu/qt6/plugins"
export PATH="$base/sysroot/usr/bin:$PATH"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
chmod 700 "$XDG_RUNTIME_DIR"
"$base/sysroot/usr/bin/kwin_wayland" --x11-display :92 --width 1366 --height 1000 --socket qa-kde-zen > "$out/logs/kwin-$1.log" 2>&1 &
kwin_pid=$!
trap 'kill "$kwin_pid" 2>/dev/null || true' EXIT
for n in {1..50}; do
 test -S "$XDG_RUNTIME_DIR/qa-kde-zen" && break
 kill -0 "$kwin_pid" || exit 1
 sleep .2
done
export WAYLAND_DISPLAY=qa-kde-zen MOZ_ENABLE_WAYLAND=1 XDG_SESSION_TYPE=wayland XDG_CURRENT_DESKTOP=KDE
python3 "$out/run.py" "$@"
