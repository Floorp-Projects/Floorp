#!/bin/bash -live

################################### build.sh ###################################

build-setup.sh

### Check that we are running as worker
test `whoami` == 'worker';

### Check that require variables are defined
test $REPOSITORY  # Should be an hg repository url to pull from
test $REVISION    # Should be an hg revision to pull down
test $MOZCONFIG   # Should be a mozconfig file from mozconfig/ folder

### Pull, Update and Build
cd /home/worker/mozilla-central;
hg pull -r $REVISION $REPOSITORY;
hg update $REVISION;
./mach build;

### Make package
cd /home/worker/object-folder;
make package package-tests;

### Extract artifacts
# Navigate to dist/ folder
cd /home/worker/object-folder/dist;
# Discard version numbers from packaged files, they just make it hard to write
# the right filename in the task payload where artifacts are declared
mv *.linux-x86_64.tar.bz2   target.linux-x86_64.tar.bz2
mv *.linux-x86_64.json      target.linux-x86_64.json
mv *.tests.zip              target.tests.zip


################################### build.sh ###################################
