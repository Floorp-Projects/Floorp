#!/bin/bash -live

################################### build.sh ###################################

. build-setup.sh

### Check that require variables are defined
test $REPOSITORY  # Should be an hg repository url to pull from
test $REVISION    # Should be an hg revision to pull down
test $MOZCONFIG   # Should be a mozconfig file from mozconfig/ folder

### Pull and update mozilla-central
cd $gecko_dir
hg pull -r $REVISION $REPOSITORY;
hg update $REVISION;

### Pull and update gaia
cd $gaia_dir
GAIA_REV=$(get_gaia_revision.js)
GAIA_REPO="https://hg.mozilla.org$(get_gaia_repo.js)"
hg pull -r $GAIA_REV $GAIA_REPO;
hg update $GAIA_REV;

cd $gecko_dir

./mach build;

### Make package
cd /home/worker/object-folder;
make package package-tests;

### Extract artifacts
# Navigate to dist/ folder
cd /home/worker/object-folder/dist;

ls -lah /home/worker/object-folder/dist/


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
