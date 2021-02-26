#!/bin/bash
source $HOME/checkouts/gecko/taskcluster/scripts/misc/source-test-common.sh

# Add clang-tidy to PATH
export PATH=$MOZ_FETCHES_DIR/clang-tidy/bin:$PATH
export PATH=$MOZ_FETCHES_DIR/cctools/bin:$MOZ_FETCHES_DIR/llvm-dsymutil/bin:$PATH

# Use toolchain clang
export LD_LIBRARY_PATH=$MOZ_FETCHES_DIR/clang/lib

# Write custom mozconfig
export MOZCONFIG=$GECKO_PATH/mozconfig

# Add to mozconfig all the appropriate options
cat <<EOT >> $MOZCONFIG
# Enable debug mode
ac_add_options --enable-debug
# Enable GC zeal, a testing and debugging feature that helps find GC-related bugs in JSAPI applications.
ac_add_options --enable-gczeal
# Do not treat warnings as errors
ac_add_options --disable-warnings-as-errors

export MACOS_SDK_DIR=$MOZ_FETCHES_DIR/MacOSX10.12.sdk

export DSYMUTIL=$GECKO_PATH/build/macosx/llvm-dsymutil
mk_add_options "export REAL_DSYMUTIL=$MOZ_FETCHES_DIR/llvm-dsymutil/bin/dsymutil"

ac_add_options --target=x86_64-apple-darwin

EOT

# Mach lookup clang-tidy in clang-tools
mkdir -p $MOZBUILD_STATE_PATH/clang-tools
ln -s $MOZ_FETCHES_DIR/clang-tidy $MOZBUILD_STATE_PATH/clang-tools/clang-tidy
