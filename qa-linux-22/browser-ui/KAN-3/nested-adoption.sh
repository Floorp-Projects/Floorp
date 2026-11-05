#!/bin/bash
set -eu
qa_root=/workspace/qa-linux-22/browser-ui
export DISPLAY=:92
export XDG_RUNTIME_DIR="$qa_root/runtime/wayland-adoption"
export XDG_CACHE_HOME="$qa_root/runtime/cache"
export XDG_CONFIG_HOME="$qa_root/runtime/config-kde-adoption"
export XDG_DATA_DIRS="$qa_root/sysroot/usr/share:/usr/local/share:/usr/share"
export LD_LIBRARY_PATH="$qa_root/sysroot/usr/lib/x86_64-linux-gnu:$qa_root/sysroot/usr/lib/x86_64-linux-gnu/weston"
export QT_PLUGIN_PATH="$qa_root/sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins:/usr/lib/x86_64-linux-gnu/qt6/plugins"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
chmod 700 "$XDG_RUNTIME_DIR"
"$qa_root/sysroot/usr/bin/kwin_wayland" --x11-display :92 --width 1366 --height 1000 --socket qa-kde-adoption > "$qa_root/environment/kwin-adoption.log" 2>&1 &
qa_kwin=$!
trap 'kill "$qa_kwin" 2>/dev/null || true' EXIT
for qa_i in {1..50}; do
 test -S "$XDG_RUNTIME_DIR/qa-kde-adoption" && break
 kill -0 "$qa_kwin" || exit 1
 sleep .2
done
export WAYLAND_DISPLAY=qa-kde-adoption
export MOZ_ENABLE_WAYLAND=1
export QA_WAYLAND=1
QA_REGRESSION_JS=wayland-old.js python3 "$qa_root/run-kan3-wayland.py" floorp 29631 regression
QA_REGRESSION_JS=wayland-new.js python3 "$qa_root/run-kan3-wayland.py" floorp 29632 regression
