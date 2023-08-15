#!/bin/sh
set -v -e -x

ncpu=-j$(grep -c ^processor /proc/cpuinfo)

WORK=/setup/
cd $WORK
git clone --depth=1 --single-branch -b system-symbols-mac https://github.com/gabrielesvelto/libdmg-hfsplus.git
cd libdmg-hfsplus
cmake .
make "$ncpu" dmg-bin hfsplus
# `make install` installs way too much stuff
cp dmg/dmg hfs/hfsplus /builds/worker/bin
strip /builds/worker/bin/dmg /builds/worker/bin/hfsplus

pip3 install --break-system-packages --no-cache-dir git+https://github.com/mozilla/reposado@3dd826dfd89c8a1224aecf381637aa0bf90a3a3c

python3 /usr/local/bin/repoutil --configure <<EOF
/opt/data-reposado/html/
/opt/data-reposado/metadata/
http://example.com/
EOF

pip3 install --break-system-packages --no-cache-dir -r /setup/requirements.txt

cd /
rm -rf /setup
