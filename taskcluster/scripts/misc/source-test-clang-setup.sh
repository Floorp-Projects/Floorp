#!/bin/bash
source $HOME/checkouts/gecko/taskcluster/scripts/misc/source-test-common.sh

# Add clang-tidy to PATH
export PATH=$MOZ_FETCHES_DIR/clang-tidy/bin:$PATH

# Use toolchain clang
export LD_LIBRARY_PATH=$MOZ_FETCHES_DIR/clang/lib

# Write custom mozconfig
export MOZCONFIG=$GECKO_PATH/mozconfig

# Add to mozconfig all the appropriate options
cat <<EOT >> $MOZCONFIG
# Enable debug mode
ac_add_options --enable-debug
# Enable clang-plugin in order to have all defines activated for static-analysis
ac_add_options --enable-clang-plugin
# Enable GC zeal, a testing and debugging feature that helps find GC-related bugs in JSAPI applications.
ac_add_options --enable-gczeal
# Do not treat warnings as errors
ac_add_options --disable-warnings-as-errors
EOT

# Mach lookup clang-tidy in clang-tools
mkdir -p $MOZBUILD_STATE_PATH/clang-tools
ln -s $MOZ_FETCHES_DIR/clang-tidy $MOZBUILD_STATE_PATH/clang-tools/clang-tidy
