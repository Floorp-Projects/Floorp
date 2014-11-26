#!/bin/bash -vex

gecko_dir=/home/worker/mozilla-central/source
gaia_dir=/home/worker/gaia/source

create_parent_dir() {
  parent_dir=$(dirname $1)
  if [ ! -d "$parent_dir" ]; then
    mkdir -p "$parent_dir"
  fi
}

### Firefox Build Setup
# Clone mozilla-central
if [ ! -d "$gecko_dir" ]; then
  create_parent_dir $gecko_dir
  hg clone https://hg.mozilla.org/mozilla-central/ $gecko_dir
fi

# Create .mozbuild so mach doesn't complain about this
mkdir -p /home/worker/.mozbuild/

# Create object-folder exists
mkdir -p /home/worker/object-folder/

### Clone gaia in too
if [ ! -d "$gaia_dir" ]; then
  create_parent_dir $gaia_dir
  hg clone https://hg.mozilla.org/integration/gaia-central/ $gaia_dir
fi
