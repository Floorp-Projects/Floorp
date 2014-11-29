#! /bin/bash -e

gecko_dir=$1
target=$2

gaia_repo=$(gaia_props.py $gecko_dir repository)
gaia_rev=$(gaia_props.py $gecko_dir revision)

# Initial clone
if [ ! -d "$target" ]; then
  echo "Running Initial gaia clone"
  mkdir -p $(dirname $target)
  tc-vcs clone $gaia_repo $target
fi

echo "Checking out gaia $gaia_repo $gaia_rev"
tc-vcs checkout-revision \
  $target \
  $gaia_repo \
  $gaia_rev \
  $gaia_rev


