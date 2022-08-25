#!/bin/sh
set -v -e -x

ncpu=-j$(grep -c ^processor /proc/cpuinfo)

WORK=/setup/
cd $WORK
git clone --depth=1 --single-branch -b system-symbols-mac https://github.com/gabrielesvelto/xar xar
cd xar/xar
./autogen.sh --prefix=/builds/worker
make "$ncpu" && make install

cd $WORK
git clone --depth=1 --single-branch -b system-symbols-mac https://github.com/gabrielesvelto/libdmg-hfsplus.git
cd libdmg-hfsplus
cmake .
make "$ncpu" dmg-bin hfsplus
# `make install` installs way too much stuff
cp dmg/dmg hfs/hfsplus /builds/worker/bin
strip /builds/worker/bin/dmg /builds/worker/bin/hfsplus

pip3 install --no-cache-dir git+https://github.com/gabrielesvelto/reposado

python3 /usr/local/bin/repoutil --configure <<EOF
/opt/data-reposado/html/
/opt/data-reposado/metadata/
http://example.com/
EOF

pip3 install --no-cache-dir -r /setup/requirements.txt

cd /
rm -rf /setup
