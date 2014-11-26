#!/bin/bash -vex

gecko_dir=/home/worker/mozilla-central/source
gaia_dir=/home/worker/gaia/source


### Firefox Build Setup
# Clone mozilla-central
if [ ! -d "$gecko_dir" ];
then
  hg clone https://hg.mozilla.org/mozilla-central/ $gecko_dir
fi

# Create .mozbuild so mach doesn't complain about this
mkdir -p /home/worker/.mozbuild/

# Create object-folder exists
mkdir -p /home/worker/object-folder/

### Clone gaia in too
if [ ! -d "$gaia_dir" ];
then
  hg clone https://hg.mozilla.org/integration/gaia-central/ $gaia_dir
fi
