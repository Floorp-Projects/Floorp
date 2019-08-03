#!/bin/bash
set -x -e -v

# This script is for building clang.

ORIGPWD="$PWD"
JSON_CONFIG="$1"

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

if [ -d "$MOZ_FETCHES_DIR/binutils/bin" ]; then
  export PATH="$MOZ_FETCHES_DIR/binutils/bin:$PATH"
fi

case "$JSON_CONFIG" in
*macosx64*)
  # these variables are used in build-clang.py
  export CROSS_CCTOOLS_PATH=$MOZ_FETCHES_DIR/cctools
  export CROSS_SYSROOT=$MOZ_FETCHES_DIR/MacOSX10.11.sdk
  export PATH=$PATH:$CROSS_CCTOOLS_PATH/bin
  ;;
*win64*)
  UPLOAD_DIR=$ORIGPWD/public/build
  # Set up all the Visual Studio paths.
  . taskcluster/scripts/misc/vs-setup.sh

  # LLVM_ENABLE_DIA_SDK is set if the directory "$ENV{VSINSTALLDIR}DIA SDK"
  # exists.
  export VSINSTALLDIR="${VSPATH}/"

  export PATH="$(cd $MOZ_FETCHES_DIR/cmake && pwd)/bin:${PATH}"
  export PATH="$(cd $MOZ_FETCHES_DIR/ninja && pwd)/bin:${PATH}"
  ;;
*linux64*|*android*)
  ;;
*)
  echo Cannot figure out build configuration for $JSON_CONFIG
  exit 1
  ;;
esac

# gets a bit too verbose here
set +x

cd $MOZ_FETCHES_DIR/llvm-project
python3 $GECKO_PATH/build/build-clang/build-clang.py -c $GECKO_PATH/$1

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang*.tar.* $UPLOAD_DIR
