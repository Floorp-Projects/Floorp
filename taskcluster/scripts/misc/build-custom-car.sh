#!/bin/bash
set -x -e -v

# This script is for building a custom version of chromium-as-release

# First argument must be the artifact name
ARTIFACT_NAME=$(basename $TOOLCHAIN_ARTIFACT)
shift

# Use the rest of the arguments as the build config for gn
CONFIG=$(echo $* | tr -d "'")

# Android build flag
if [[ "$ARTIFACT_NAME" == *"android"* ]]; then
  IS_ANDROID=true
fi

mkdir custom_car
cd custom_car
CUSTOM_CAR_DIR=$PWD

# Setup depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$PATH:$CUSTOM_CAR_DIR/depot_tools"

# Log the current revision of depot tools for easier tracking in the future
DEPOT_TOOLS_REV=$(cd depot_tools && git rev-parse HEAD && cd ..)
echo "Current depot_tools revision: $DEPOT_TOOLS_REV"

# Set up some env variables depending on the target OS
# Linux is the default case, with minor adjustments for
# android since it is built with a linux host

# Final folder structure before compressing is
# the same for linux and windows
FINAL_BIN_PATH="src/out/Default"

# Final binary name for autoninja build sequence
FINAL_BIN=chrome

# Unique substring for PGO data for Linux
PGO_SUBSTR="chrome-linux-main"

# Default (non android) fetch name for upstream src
FETCH_NAME=chromium

# Android specific vars
if [ "$IS_ANDROID" = true ]; then
  FETCH_NAME=android
  PGO_SUBSTR="android64"
  FINAL_BIN_PATH="src/out/Default/apks"
  FINAL_BIN=chrome_public_apk
fi

# Logic for macosx64
if [[ $(uname -s) == "Darwin" ]]; then
  # Modify the config with fetched sdk path
  export MACOS_SYSROOT="$MOZ_FETCHES_DIR/MacOSX14.2.sdk"

  # Avoid mixing up the system python and toolchain python in the
  # python path configuration
  # https://bugs.python.org/issue22490
  unset __PYVENV_LAUNCHER__

  # Set the SDK path for build, which is technically a higher version
  # than what is associated with the current OS version (10.15).
  # This should work as long as MACOSX_DEPLOYMENT_TARGET is set correctly
  CONFIG=$(echo $CONFIG mac_sdk_path='"'$MACOS_SYSROOT'"')

  # Ensure we don't use ARM64 profdata with this unique sub string
  PGO_SUBSTR="chrome-mac-main"

  # Temporary hacky way for now while we build this on intel workers.
  # Afterwards we can replace it with a $(uname -m) == "arm64" check.
  # Bug 1858740
  if [[ "$ARTIFACT_NAME" == *"macosx_arm"* ]]; then
    PGO_SUBSTR="chrome-mac-arm-main"
  fi

  # macOS final build folder is different than linux/win
  FINAL_BIN_PATH="src/out/Default/Chromium.app"
fi

# Logic for win64 using the mingw environment
if [[ $(uname -o) == "Msys" ]]; then
  # Setup VS 2022
  . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

  # Setup some environment variables for chromium build scripts
  export DEPOT_TOOLS_WIN_TOOLCHAIN=0
  export GYP_MSVS_OVERRIDE_PATH="$MOZ_FETCHES_DIR/VS"
  export GYP_MSVS_VERSION=2022
  export vs2022_install="$MOZ_FETCHES_DIR/VS"
  export WINDOWSSDKDIR="$MOZ_FETCHES_DIR/VS/Windows Kits/10"
  export DEPOT_TOOLS_UPDATE=1
  export GCLIENT_PY3=1
  # Fool GYP
  touch "$MOZ_FETCHES_DIR/VS/VC/vcvarsall.bat"

  # Construct some of our own dirs and move VS dlls + other files
  # to a path that chromium build files & scripts are expecting
  mkdir chrome_dll
  cd chrome_dll
  mkdir system32
  cd ../
  pushd "$WINDOWSSDKDIR"
  mkdir -p Debuggers/x64/
  popd
  mv $MOZ_FETCHES_DIR/VS/VC/Redist/MSVC/14.38.33130/x64/Microsoft.VC143.CRT/* chrome_dll/system32/
  mv "$WINDOWSSDKDIR/App Certification Kit/"* "$WINDOWSSDKDIR"/Debuggers/x64/
  export WINDIR="$PWD/chrome_dll"

  # Run glcient once first to get some windows deps
  gclient

  # Ensure we don't use WIN32 profdata with this unique sub string
  PGO_SUBSTR="chrome-win64-main"
fi

# Get chromium source code and dependencies
mkdir chromium
cd chromium

fetch --no-history --nohooks $FETCH_NAME

# Setup the .gclient file to ensure pgo profiles are downloaded.
# For some reason we need to set --name flag even though it already exists.
# Currently the gclient.py file does NOT recognize --custom-var as it's own argument
gclient config --name src "https://chromium.googlesource.com/chromium/src.git" --custom-var="checkout_pgo_profiles=True" --unmanaged

cd src

# Log the current revision of the chromium src for easier tracking in the future
CHROMIUM_REV=$(git rev-parse HEAD)
echo "Current chromium revision: $CHROMIUM_REV"

# Amend gclient file
if [ "$IS_ANDROID" = true ]; then
  echo "target_os = [ 'android' ]" >> ../.gclient
fi

if [[ $(uname -o) == "Msys" ]]; then
  # For fast fetches it seems we will be missing some dummy files in windows.
  # We can create a dummy this way to satisfy the rest of the build sequence.
  # This is ok because we are not doing any development here and don't need
  # the development history, but this file is still needed to proceed.
  python3 build/util/lastchange.py -o build/util/LASTCHANGE
fi

if [[ $(uname -s) == "Linux" ]] || [[ $(uname -s) == "Darwin" ]]; then
  # Bug 1847210
  # Modifications to how the dirname and depot_tools and other env variables
  # change how cipd is setup for Mac and Linux.
  # Easily resolved by just running the setup script.
  source ./third_party/depot_tools/cipd_bin_setup.sh
  cipd_bin_setup
fi

# Sync again for android, after cipd bin setup
if [ "$IS_ANDROID" = true ]; then
  gclient sync
fi

# Now we can run hooks and fetch PGO + everything else
gclient runhooks

# PGO data should be in src/chrome/build/pgo_profiles/ 
# with a name like "chrome-{OS}-<some unique identifier>"
export PGO_DATA_DIR="$CUSTOM_CAR_DIR/chromium/src/chrome/build/pgo_profiles"
for entry in "$PGO_DATA_DIR"/*
do
  if [ -f "$entry" ]; then
    if [[ "$entry" == *"$PGO_SUBSTR"* ]]; then
        echo "Found the correct profdata"
        export PGO_DATA_PATH="$entry"
        break
    fi
  fi
done

PGO_FILE=$PGO_DATA_PATH
if [[ $(uname -o) == "Msys" ]]; then
  # Compute a relative path that the build scripts looks for.
  # This odd pathing seems to only happen on windows
  PGO_FILE=${PGO_DATA_PATH#*/*/*/*/*/*/*/*/*/}
  mv $PGO_DATA_PATH build/config/compiler/pgo/
fi

CONFIG=$(echo $CONFIG pgo_data_path='"'$PGO_FILE'"')

# Set up then build chrome
gn gen out/Default --args="$CONFIG"
autoninja -C out/Default $FINAL_BIN

# Gather binary and related files into a zip, and upload it
cd ..
mkdir chromium

mv "$FINAL_BIN_PATH" chromium
chmod -R +x chromium

tar -c chromium | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > $ARTIFACT_NAME

mkdir -p $UPLOAD_DIR
mv "$ARTIFACT_NAME" "$UPLOAD_DIR"
