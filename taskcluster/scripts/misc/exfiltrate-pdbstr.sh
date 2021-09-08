#!/bin/sh

set -x -e -v

# Copy the file that already exists on Windows workers.
mkdir pdbstr
cp "c:/Program Files (x86)/Windows Kits/10/Debuggers/$1/srcsrv/pdbstr.exe" pdbstr

tar -c pdbstr | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > pdbstr.tar.zst
