#!/bin/bash -live

################################### build-mulet-linux.sh ###################################

. build-setup.sh

### Check that require variables are defined
test $REPOSITORY  # Should be an hg repository url to pull from
test $REVISION    # Should be an hg revision to pull down
test $MOZCONFIG   # Should be a mozconfig file from mozconfig/ folder

### Pull and update mozilla-central
cd $gecko_dir
hg pull -r $REVISION $REPOSITORY;
hg update $REVISION;

### Retrieve and install latest tooltool manifest
tooltool=/home/worker/tools/tooltool.py
manifest=browser/config/tooltool-manifests/linux64/releng.manifest
tooltool_url=http://tooltool.pub.build.mozilla.org/temp-sm-stuff

python $tooltool --url $tooltool_url --overwrite -m $manifest fetch -c $TOOLTOOL_CACHE
chmod +x setup.sh
./setup.sh

export MOZ_OBJDIR=$(get-objdir.py $gecko_dir)

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

################################### build.sh ###################################
