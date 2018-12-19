#!/bin/bash
set -x -e -v

# This script is for building a custom version of V8
ARTIFACT_NAME='d8.zip'
CONFIG='is_debug=false target_cpu="x64"'
if [[ $# -eq 0 ]]; then
    echo "Using default configuration for v8 build."
    CONFIG=$(echo $CONFIG | tr -d "'")
else
	# First argument must be the artifact name
	ARTIFACT_NAME="$1"
	shift

	# Use the rest of the arguments as the build config
	CONFIG=$(echo $* | tr -d "'")
fi

echo "Config: $CONFIG"
echo "Artifact name: $ARTIFACT_NAME"

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

cd $HOME_DIR/src

# Setup depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:$HOME_DIR/src/depot_tools
gclient

# Get v8 source code
mkdir v8
cd v8
fetch v8
cd v8

# Build dependencies
gclient sync
./build/install-build-deps.sh

# Build v8
git checkout master
git pull && gclient sync

gn gen out/release --args="$CONFIG"
ninja -C out/release d8

# Gather binary and related files into a zip, and upload it
cd ..
mkdir d8

cp -R v8/out/release d8
cp -R v8/include d8
chmod -R +x d8

zip -r $ARTIFACT_NAME d8

mkdir -p $UPLOAD_DIR
cp $ARTIFACT_NAME $UPLOAD_DIR
