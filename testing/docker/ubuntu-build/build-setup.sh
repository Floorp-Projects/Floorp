#!/usr/bin/env bash

set -ve

test `whoami` == 'root';

# run mozbootstrap to install build specific dependencies
wget -q https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py
python bootstrap.py --application-choice=desktop --no-interactive

# note that TC will replace workspace with a cache mount; there's no sense
# creating anything inside there
mkdir -p /home/worker/workspace
chown worker:worker /home/worker/workspace

# /builds is *not* replaced with a mount in the docker container. The worker
# user writes to lots of subdirectories, though, so it's owned by that user
mkdir -p /builds
chown worker:worker /builds

# install tooltool directly from github where tooltool_wrapper.sh et al. expect
# to find it
wget -O /builds/tooltool.py https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py
chmod +x /builds/tooltool.py

# check out the tools repo; this will be updated as necessary in each container
# but it changes infrequently so it makes sense to cache in place
tc-vcs checkout /builds/tools https://hg.mozilla.org/build/tools
chown -R worker:worker /builds/tools

rm /tmp/build-setup.sh
