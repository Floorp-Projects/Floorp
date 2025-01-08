#!/bin/bash
set -x -e -v

# This script is for building a custom version of V8
ARTIFACT_NAME='d8.tar.zst'
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

cd $GECKO_PATH

# Setup depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:$GECKO_PATH/depot_tools
# Bug 1901936 changes to config upstream for depot tools path
export XDG_CONFIG_HOME=$GECKO_PATH

# Get v8 source code and dependencies
fetch --force v8
cd v8

# Build v8
gn gen out/release --args="$CONFIG"
ninja -C out/release d8

# Gather binary and related files into a zip, and upload it
cd ..
mkdir d8

cp -R v8/out/release d8
cp -R v8/include d8
chmod -R +x d8

tar caf $ARTIFACT_NAME d8

mkdir -p $UPLOAD_DIR
cp $ARTIFACT_NAME $UPLOAD_DIR
