#!/bin/sh
set -v -e -x

ncpu=-j$(grep -c ^processor /proc/cpuinfo)

WORK=/setup/
cd $WORK
git clone https://github.com/mackyle/xar xar
cd xar/xar
./autogen.sh --prefix=/home/worker
make "$ncpu" && make install

cd $WORK
git clone -b from_zarvox https://github.com/andreas56/libdmg-hfsplus.git
cd libdmg-hfsplus
cmake .
make "$ncpu" dmg-bin hfsplus
# `make install` installs way too much stuff
cp dmg/dmg hfs/hfsplus /home/worker/bin
strip /home/worker/bin/dmg /home/worker/bin/hfsplus

cd $WORK
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:$PWD/depot_tools
mkdir breakpad
cd breakpad
fetch breakpad
cd src
touch README
./configure
make "$ncpu" src/tools/mac/dump_syms/dump_syms_mac
# `make install` is broken because there are two dump_syms binaries.
cp src/tools/mac/dump_syms/dump_syms_mac /home/worker/bin
strip /home/worker/bin/dump_syms_mac

pip install git+https://github.com/wdas/reposado
# Patch repo_sync from reposado for
# https://github.com/wdas/reposado/pull/34
cp /setup/repo_sync /usr/local/bin

repoutil --configure <<EOF
/opt/data-reposado/html/
/opt/data-reposado/metadata/
http://example.com/
EOF

pip install -r /setup/requirements.txt

cd /
rm -rf /setup
