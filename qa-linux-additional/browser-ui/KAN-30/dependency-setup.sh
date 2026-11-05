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
