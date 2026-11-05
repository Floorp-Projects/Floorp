#!/bin/bash
set -euo pipefail
r=/workspace/qa-linux-22/browser-ui/KAN-29/pr2877
b=/workspace/qa-linux-22/browser-ui
export DISPLAY=:92 XDG_RUNTIME_DIR="$r/runtime/wayland" XDG_CACHE_HOME="$r/runtime/cache" XDG_CONFIG_HOME="$r/runtime/config"
export XDG_DATA_DIRS="$b/sysroot/usr/share:/usr/local/share:/usr/share"
export LD_LIBRARY_PATH="$b/sysroot/usr/lib/x86_64-linux-gnu:$b/sysroot/usr/lib/x86_64-linux-gnu/weston"
export QT_PLUGIN_PATH="$b/sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins:/usr/lib/x86_64-linux-gnu/qt6/plugins"
export PATH="$b/sysroot/usr/bin:$PATH"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CACHE_HOME" "$XDG_CONFIG_HOME" "$r/logs"
chmod 700 "$XDG_RUNTIME_DIR"
"$b/sysroot/usr/bin/kwin_wayland" --x11-display :92 --width 1366 --height 1000 --socket qa-kde-pr2877-tests-$1 > "$r/logs/kwin-$1.log" 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
for i in {1..100};do test -S "$XDG_RUNTIME_DIR/qa-kde-pr2877-tests-$1" && break;kill -0 "$pid";sleep .2;done
export WAYLAND_DISPLAY=qa-kde-pr2877-tests-$1 MOZ_ENABLE_WAYLAND=1 XDG_SESSION_TYPE=wayland XDG_CURRENT_DESKTOP=KDE
python3 "$r/run-tests.py" "$@"
