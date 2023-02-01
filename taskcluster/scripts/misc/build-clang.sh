#!/bin/bash
set -x -e -v

# This script is for building clang.

ORIGPWD="$PWD"
CONFIGS=$(for c; do echo -n " -c $GECKO_PATH/$c"; done)

cd $GECKO_PATH

if [ -d "$MOZ_FETCHES_DIR/binutils/bin" ]; then
  export PATH="$MOZ_FETCHES_DIR/binutils/bin:$PATH"
fi

# Make the installed compiler-rt(s) available to clang.
UPLOAD_DIR= taskcluster/scripts/misc/repack-clang.sh

case "$CONFIGS" in
*macosx64*)
  # cmake makes decisions based on the output of the mac-only sw_vers, which is
  # obviously missing when cross-compiling, so create a fake one. The exact
  # version doesn't really matter: as of writing, cmake checks at most for 10.5.
  mkdir -p $ORIGPWD/bin
  echo "#!/bin/sh" > $ORIGPWD/bin/sw_vers
  echo echo 10.12 >> $ORIGPWD/bin/sw_vers
  chmod +x $ORIGPWD/bin/sw_vers
  # these variables are used in build-clang.py
  export CROSS_SYSROOT=$(ls -d $MOZ_FETCHES_DIR/MacOSX1*.sdk)
  export PATH=$PATH:$ORIGPWD/bin
  ;;
*win64*)
  case "$(uname -s)" in
  MINGW*|MSYS*)
    export UPLOAD_DIR=$ORIGPWD/public/build
    # Set up all the Visual Studio paths.
    . taskcluster/scripts/misc/vs-setup.sh

    # LLVM_ENABLE_DIA_SDK is set if the directory "$ENV{VSINSTALLDIR}DIA SDK"
    # exists.
    export VSINSTALLDIR="${VSPATH}/"

    export PATH="$(cd $MOZ_FETCHES_DIR/cmake && pwd)/bin:${PATH}"
    export PATH="$(cd $MOZ_FETCHES_DIR/ninja && pwd)/bin:${PATH}"
    ;;
  *)
    export VSINSTALLDIR="$MOZ_FETCHES_DIR/vs"
    ;;
  esac
  ;;
*linux64*|*android*)
  ;;
*)
  echo Cannot figure out build configuration for $CONFIGS
  exit 1
  ;;
esac

# gets a bit too verbose here
set +x

cd $MOZ_FETCHES_DIR/llvm-project
python3 $GECKO_PATH/build/build-clang/build-clang.py $CONFIGS

set -x

if [ -f clang*.tar.zst ]; then
    # Put a tarball in the artifacts dir
    mkdir -p $UPLOAD_DIR
    cp clang*.tar.zst $UPLOAD_DIR
fi

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
