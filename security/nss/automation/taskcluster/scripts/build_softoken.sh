#!/usr/bin/env bash

set -v -e -x

source $(dirname $0)/tools.sh
source $(dirname $0)/split.sh

test -d dist-util || { echo "run build_util.sh first" 1>&2; exit 1; }

rm -rf nss-softoken
split_softoken nss nss-softoken

# Build.
platform=`make -s -C nss platform`
export LIBRARY_PATH="$PWD/dist-nspr/$platform/lib:$PWD/dist-util/$platform/lib"
export LD_LIBRARY_PATH="$LIBRARY_PATH:$LD_LIBRARY_PATH"
export INCLUDES="-I$PWD/dist-nspr/$platform/include -I$PWD/dist-util/public/nss"
export NSS_BUILD_SOFTOKEN_ONLY=1

rm -rf dist
make -C nss-softoken nss_build_all

for i in blapi alghmac cmac; do
    mv "dist/private/nss/${i}.h" dist/public/nss
done

# Package.
test -d artifacts || mkdir artifacts
rm -rf dist-softoken
mv dist dist-softoken
tar cvfjh artifacts/dist-softoken.tar.bz2 dist-softoken
