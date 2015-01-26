#!/bin/bash -vex

gecko_dir=/home/worker/gecko/source
gaia_dir=/home/worker/gaia/source

create_parent_dir() {
  parent_dir=$(dirname $1)
  if [ ! -d "$parent_dir" ]; then
    mkdir -p "$parent_dir"
  fi
}

# Ensure we always have the parent directory for gecko
create_parent_dir $gecko_dir

# Create .mozbuild so mach doesn't complain about this
mkdir -p /home/worker/.mozbuild/

# Create object-folder exists
mkdir -p /home/worker/object-folder/
