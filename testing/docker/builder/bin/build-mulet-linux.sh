#!/bin/bash -live

################################### build-mulet-linux.sh ###################################

. build-setup.sh

### Check that require variables are defined
test $MOZCONFIG

# Ensure gecko is at the correct revision
pull-gecko.sh $gecko_dir

### Install package dependencies
install-packages.sh $gecko_dir

export MOZ_OBJDIR=$(get-objdir.py $gecko_dir)

cd $gecko_dir
./mach build;

### Make package
cd $MOZ_OBJDIR;
make package package-tests;

### Extract artifacts
# Navigate to dist/ folder
cd $MOZ_OBJDIR/dist;

ls -lah $MOZ_OBJDIR/dist/


# Target names are cached so make sure we discard them first if found.
rm -f target.linux-x86_64.tar.bz2 target.linux-x86_64.json target.tests.zip

# Artifacts folder is outside of the cache.
mkdir -p /home/worker/artifacts/

# Discard version numbers from packaged files, they just make it hard to write
# the right filename in the task payload where artifacts are declared
mv *.linux-x86_64.tar.bz2   /home/worker/artifacts/target.linux-x86_64.tar.bz2
mv *.linux-x86_64.json      /home/worker/artifacts/target.linux-x86_64.json
mv *.tests.zip              /home/worker/artifacts/target.tests.zip
mv jsshell-linux-x86_64.zip /home/worker/artifacts/jsshell-linux-x86_64.zip

################################### build.sh ###################################
