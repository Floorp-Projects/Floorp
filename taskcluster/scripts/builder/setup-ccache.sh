#! /bin/bash -ex

test -d $1 # workspace must exist at this point...
WORKSPACE=$( cd "$1" && pwd )

export CCACHE_DIR=$WORKSPACE/ccache

ccache -M 12G
ccache -s
