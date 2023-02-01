#!/bin/bash

set -x
set -e
set -o pipefail

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}

$GECKO_PATH/mach python $(dirname $0)/unpack-sdk.py "$@" $dir

$(dirname $0)/pack.sh $dir
