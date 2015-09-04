#!/bin/bash -ex

################################### build.sh ###################################
# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

. desktop-setup.sh

### Clone gaia
pull-gaia.sh $GECKO_DIR $GAIA_DIR

# Ensure symlink has been created to gaia...
rm -f $GECKO_DIR/gaia
ln -s $GAIA_DIR $GECKO_DIR/gaia

cd $GECKO_DIR
./mach build;

### Make package
cd $MOZ_OBJDIR
make package package-tests buildsymbols

### Extract artifacts
# Navigate to dist/ folder
cd $MOZ_OBJDIR/dist
ls -lah $MOZ_OBJDIR/dist/

# Target names are cached so make sure we discard them first if found.
rm -f target.linux-x86_64.tar.bz2 target.linux-x86_64.json target*.tests.zip

# Artifacts folder is outside of the cache.
mkdir -p $HOME/artifacts/

# Discard version numbers from packaged files, they just make it hard to write
# the right filename in the task payload where artifacts are declared
mv *.linux-x86_64.tar.bz2       $HOME/artifacts/target.linux-x86_64.tar.bz2
mv *.linux-x86_64.json          $HOME/artifacts/target.linux-x86_64.json
for name in common cppunittest reftest mochitest xpcshell web-platform; do
    mv *.$name.tests.zip          $HOME/artifacts/target.$name.tests.zip ;
done
mv test_packages_tc.json        $HOME/artifacts/test_packages.json
mv *.crashreporter-symbols.zip  $HOME/artifacts/target.crashreporter-symbols.zip
mv mozharness.zip               $HOME/artifacts/mozharness.zip

# If the simulator does not exist don't fail
mv fxos-simulator*              $HOME/artifacts/fxos-simulator.xpi || :

ccache -s

################################### build.sh ###################################
