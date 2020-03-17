#!/bin/sh

set -x -e -v

# Copy the file that already exists on Windows workers.
mkdir pdbstr
cp "c:/Program Files (x86)/Windows Kits/10/Debuggers/$1/srcsrv/pdbstr.exe" pdbstr

tar -jcvf pdbstr.tar.bz2 pdbstr
