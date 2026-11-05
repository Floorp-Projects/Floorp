#!/bin/bash
set -eu
qa_root=/workspace/qa-linux-22/browser-ui
export DISPLAY=:92
export XDG_RUNTIME_DIR="$qa_root/runtime/wayland-product3033"
export XDG_CACHE_HOME="$qa_root/runtime/cache"
export XDG_CONFIG_HOME="$qa_root/runtime/config-kde-adoption"
export XDG_DATA_DIRS="$qa_root/sysroot/usr/share:/usr/local/share:/usr/share"
export LD_LIBRARY_PATH="$qa_root/sysroot/usr/lib/x86_64-linux-gnu:$qa_root/sysroot/usr/lib/x86_64-linux-gnu/weston"
export QT_PLUGIN_PATH="$qa_root/sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins:/usr/lib/x86_64-linux-gnu/qt6/plugins"
mkdir -p "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
chmod 700 "$XDG_RUNTIME_DIR"
"$qa_root/sysroot/usr/bin/kwin_wayland" --x11-display :92 --width 1366 --height 1000 --socket qa-kde-product3033 > "$qa_root/environment/kwin-product3033.log" 2>&1 &
qa_kwin=$!
trap 'kill "$qa_kwin" 2>/dev/null || true' EXIT
for qa_i in {1..50}; do
 test -S "$XDG_RUNTIME_DIR/qa-kde-product3033" && break
 kill -0 "$qa_kwin" || exit 1
 sleep .2
done
export WAYLAND_DISPLAY=qa-kde-product3033
export MOZ_ENABLE_WAYLAND=1
export QA_WAYLAND=1
QA_CANDIDATE="$qa_root/full-product/3033/package-extracted/floorp/floorp" python3 "$qa_root/full-product/3033/run-candidate.py" floorp 29832 split
