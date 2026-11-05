#!/bin/sh
set -eu
# Optional QA_WM=twm only. Official Debian artifacts; do not install globally.
mkdir -p deps/root
curl -fLsS https://deb.debian.org/debian/pool/main/t/twm/twm_1.0.10-1+b1_amd64.deb -o deps/twm.deb
curl -fLsS https://deb.debian.org/debian/pool/main/x/xfonts-base/xfonts-base_1.0.5+nmu1_all.deb -o deps/xfonts-base.deb
dpkg-deb -x deps/twm.deb deps/root
dpkg-deb -x deps/xfonts-base.deb deps/root
mkfontdir deps/root/usr/share/fonts/X11/misc
# Compare dependency hashes against wm-control.json before running QA_WM=twm.

# EWMH follow-up dependencies (Debian13 ABI); scoped LD_LIBRARY_PATH/XDG_DATA_DIRS in run.py.
for name in openbox libobrender32v5 libobt2v5; do
 curl -fLsS "https://deb.debian.org/debian/pool/main/o/openbox/${name}_3.6.1-12+b2_amd64.deb" -o "deps/${name}-ewmh.deb"
 dpkg-deb -x "deps/${name}-ewmh.deb" deps/root
done
curl -fLsS https://deb.debian.org/debian/pool/main/i/imlib2/libimlib2t64_1.12.4-2_amd64.deb -o deps/imlib-ewmh.deb
dpkg-deb -x deps/imlib-ewmh.deb deps/root
