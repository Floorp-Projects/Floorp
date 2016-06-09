#! /bin/bash -e

# Ensure all the scripts in this dir are on the path....
DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
PATH=$DIRNAME:$PATH

gecko_dir=$1
target=$2
gaia_props=$3

gaia_repo=$($gaia_props $gecko_dir repository)
gaia_rev=$($gaia_props $gecko_dir revision)

tc-vcs checkout $target $gaia_repo $gaia_repo $gaia_rev
