#! /bin/bash -e

gecko_dir=$1
target=$2

gaia_repo=$(gaia_props.py $gecko_dir repository)
gaia_rev=$(gaia_props.py $gecko_dir revision)

tc-vcs checkout $target $gaia_repo $gaia_repo $gaia_rev
