#!/bin/bash -ex

################################### build-mulet-linux.sh ###################################
# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

. desktop-setup.sh

# use "simple" package names so that they can be hard-coded in the task's
# extras.locations
export MOZ_SIMPLE_PACKAGE_NAME=target

cd $GECKO_DIR
./mach build;

### Make package
cd $MOZ_OBJDIR;
make package package-tests buildsymbols;

### Extract artifacts
# Navigate to dist/ folder
cd $MOZ_OBJDIR/dist;

ls -lah $MOZ_OBJDIR/dist/

# Artifacts folder is outside of the cache.
mkdir -p $HOME/artifacts/

# Discard version numbers from packaged files, they just make it hard to write
# the right filename in the task payload where artifacts are declared
mv target.*                     $HOME/artifacts
mv mozharness.zip               $HOME/artifacts/mozharness.zip

ccache -s

################################### build.sh ###################################
