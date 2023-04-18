#!/bin/bash

set -x
set -e
set -o pipefail

script=$1
shift
artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}

$GECKO_PATH/mach python --virtualenv build $(dirname $0)/$script "$@" $dir

$(dirname $0)/pack.sh $dir
