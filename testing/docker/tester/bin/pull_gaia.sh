#!/bin/bash -vex


gaia_dir=/home/worker/gaia

# Some mozharness scripts are harcoded to use $PWD/gaia for the gaia repo and 
# will delete the directory and clone again if .hg doesn't exist.  Does not work
# well when trying to cache
if [ ! -d "$gaia_dir/.hg" ]; then
  echo "Cloning gaia into $gaia_dir"
  hg clone https://hg.mozilla.org/integration/gaia-central/ $gaia_dir
fi
