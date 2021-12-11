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

cd $WORK
git clone --depth=1 --single-branch https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:$PWD/depot_tools
mkdir breakpad
cd breakpad
fetch breakpad
cd src
touch README
./configure
make "$ncpu" src/tools/mac/dump_syms/dump_syms_mac
# `make install` is broken because there are two dump_syms binaries.
cp src/tools/mac/dump_syms/dump_syms_mac /builds/worker/bin
strip /builds/worker/bin/dump_syms_mac

pip3 install --no-cache-dir git+https://github.com/gabrielesvelto/reposado

python3 /usr/local/bin/repoutil --configure <<EOF
/opt/data-reposado/html/
/opt/data-reposado/metadata/
http://example.com/
EOF

pip3 install --no-cache-dir -r /setup/requirements.txt

cd /
rm -rf /setup
