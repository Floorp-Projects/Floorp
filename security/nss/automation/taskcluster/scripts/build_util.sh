#!/usr/bin/env bash

set -v -e -x

source $(dirname $0)/tools.sh
source $(dirname $0)/split.sh

rm -rf nss-util
split_util nss nss-util

# Build.
platform=`make -s -C nss platform`
export LIBRARY_PATH="$PWD/dist-nspr/$platform/lib"
export LD_LIBRARY_PATH="$LIBRARY_PATH:$LD_LIBRARY_PATH"
export INCLUDES="-I$PWD/dist-nspr/$platform/include"
export NSS_BUILD_UTIL_ONLY=1

rm -rf dist
make -C nss-util nss_build_all

# Package.
test -d artifacts || mkdir artifacts
rm -rf dist-util
mv dist dist-util
tar cvfjh artifacts/dist-util.tar.bz2 dist-util
