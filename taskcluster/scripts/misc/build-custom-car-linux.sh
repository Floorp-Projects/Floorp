#!/bin/bash
set -x -e -v

# This script is for building a custom version of chromium-as-release on Linux

# First argument must be the artifact name
ARTIFACT_NAME=$(basename $TOOLCHAIN_ARTIFACT)
shift

# Use the rest of the arguments as the build config
CONFIG=$(echo $* | tr -d "'")


mkdir custom_car
cd custom_car
CUSTOM_CAR_DIR=$PWD

# Setup depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$PATH:$CUSTOM_CAR_DIR/depot_tools


# Get chromium source code and dependencies
mkdir chromium
cd chromium
fetch --no-history --nohooks chromium

# setup the .gclient file to ensure pgo profiles are downloaded 
# for some reason we need to set --name flag even though it already exists.
# currently the gclient.py file does NOT recognize --custom-var as it's own argument
gclient config --name src "https://chromium.googlesource.com/chromium/src.git" --custom-var="checkout_pgo_profiles=True" --unmanaged

cd src

# need sudo access to run this and get required dependencies for linux
./build/install-build-deps.sh

# now we can run hooks and fetch PGO + everything else
gclient runhooks

# PGO data should be in src/chrome/build/pgo_profiles/ 
# with a name like "chrome-{OS}-<some unique identifier>"
export PGO_DATA_DIR="$CUSTOM_CAR_DIR/chromium/src/chrome/build/pgo_profiles"
for entry in "$PGO_DATA_DIR"/*
do
  if [ -f "$entry" ];then
    export PGO_DATA_PATH="$entry"
  fi
done
CONFIG=$(echo $CONFIG pgo_data_path='"'$PGO_DATA_PATH'"')


# set up then build chrome
gn gen out/Default --args="$CONFIG"
autoninja -C out/Default chrome # skips test binaries


# Gather binary and related files into a zip, and upload it
cd ..
mkdir chromium

mv src/out/Default chromium
chmod -R +x chromium

tar caf $ARTIFACT_NAME chromium

mkdir -p $UPLOAD_DIR
mv $ARTIFACT_NAME $UPLOAD_DIR
