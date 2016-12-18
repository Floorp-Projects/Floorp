#!/bin/bash
set -x -e -v

# This script is for building clang for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$WORKSPACE/artifacts

# Fetch our toolchain from tooltool
cd $HOME_DIR
wget -O tooltool.py https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py
chmod +x tooltool.py
: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE
cd src
$HOME_DIR/tooltool.py -m browser/config/tooltool-manifests/linux64/releng.manifest fetch

# gets a bit too verbose here
set +x

cd build/build-clang
# |mach python| sets up a virtualenv for us!
../../mach python ./build-clang.py -c clang-tidy-linux64.json

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.* $UPLOAD_DIR
