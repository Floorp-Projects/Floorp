#!/bin/bash
set -x -e -v

# This script is for building a custom version of chromium-as-release on Windows

# First argument must be the artifact name
ARTIFACT_NAME=$(basename $TOOLCHAIN_ARTIFACT)
shift

# Use the rest of the arguments as the build config
CONFIG=$(echo $* | tr -d "'")

mkdir custom_car
cd custom_car
CUSTOM_CAR_DIR=$PWD

# setup VS 2022
. $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

# setup depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$(cd ./depot_tools && pwd):$PATH"

# setup some environment variables for chromium build scripts
export DEPOT_TOOLS_WIN_TOOLCHAIN=0
export GYP_MSVS_OVERRIDE_PATH="$MOZ_FETCHES_DIR/VS"
export GYP_MSVS_VERSION=2022
export vs2022_install="$MOZ_FETCHES_DIR/VS"
export WINDOWSSDKDIR="$MOZ_FETCHES_DIR/VS/Windows Kits/10"
export DEPOT_TOOLS_UPDATE=1
export GCLIENT_PY3=1
# Fool GYP
touch "$MOZ_FETCHES_DIR/VS/VC/vcvarsall.bat"


# construct some of our own dirs and move VS dlls + other files
# to a path that chromium build files are expecting
mkdir chrome_dll
cd chrome_dll
mkdir system32
cd ../
pushd "$WINDOWSSDKDIR"
mkdir -p Debuggers/x64/
popd
mv $MOZ_FETCHES_DIR/VS/VC/Redist/MSVC/14.34.31931/x64/Microsoft.VC143.CRT/* chrome_dll/system32/
mv "$WINDOWSSDKDIR/App Certification Kit/"* "$WINDOWSSDKDIR"/Debuggers/x64/
export WINDIR="$PWD/chrome_dll"

# run glcient once first to get some windows deps
gclient

# fetch chromium src code
mkdir chromium
cd chromium
fetch --no-history --nohooks chromium

# setup the .gclient file to ensure pgo profiles are downloaded 
# for some reason we need to set --name flag even though it already exists.
# currently the gclient.py file does NOT recognize --custom-var as it's own argument
gclient config --name src https://chromium.googlesource.com/chromium/src.git --custom-var=checkout_pgo_profiles=True --unmanaged

cd src

# For fast fetches it seems we will be missing some dummy files in windows.
# We can create a dummy this way to satisfy the rest of the build sequence
# this is ok because we are not doing any development here and don't need 
# history.
python3 build/util/lastchange.py -o build/util/LASTCHANGE

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
# compute a relative path that the build scripts looks for.
# this odd pathing seems to only happen on windows
PGO_FILE=${PGO_DATA_PATH#*/*/*/*/*/*/*/*/*/}
mv $PGO_DATA_PATH build/config/compiler/pgo/
CONFIG=$(echo $CONFIG pgo_data_path='"'$PGO_FILE'"')

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
