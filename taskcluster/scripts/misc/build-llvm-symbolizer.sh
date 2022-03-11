#!/bin/sh

set -e -x

$(dirname $0)/build-llvm-common.sh llvm install-llvm-symbolizer "$@"
