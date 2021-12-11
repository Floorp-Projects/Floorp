#!/usr/bin/env bash

set -v -e -x

source $(dirname $0)/tools.sh
source $(dirname $0)/split.sh

test -d dist-softoken || { echo "run build_softoken.sh first" 1>&2; exit 1; }

rm -rf nss-nss
split_nss nss nss-nss

# Build.
export NSS_BUILD_WITHOUT_SOFTOKEN=1
export NSS_USE_SYSTEM_FREEBL=1

platform=`make -s -C nss platform`

export NSPR_LIB_DIR="$PWD/dist-nspr/$platform/lib"
export NSSUTIL_LIB_DIR="$PWD/dist-util/$platform/lib"
export FREEBL_LIB_DIR="$PWD/dist-softoken/$platform/lib"
export SOFTOKEN_LIB_DIR="$PWD/dist-softoken/$platform/lib"
export FREEBL_LIBS=-lfreebl

export NSS_NO_PKCS11_BYPASS=1
export FREEBL_NO_DEPEND=1

export LIBRARY_PATH="$PWD/dist-nspr/$platform/lib:$PWD/dist-util/$platform/lib:$PWD/dist-softoken/$platform/lib"
export LD_LIBRARY_PATH="$LIBRARY_PATH:$LD_LIBRARY_PATH"
export INCLUDES="-I$PWD/dist-nspr/$platform/include -I$PWD/dist-util/public/nss -I$PWD/dist-softoken/public/nss"

rm -rf dist
make -C nss-nss nss_build_all

# Package.
test -d artifacts || mkdir artifacts
rm -rf dist-nss
mv dist dist-nss
tar cvfjh artifacts/dist-nss.tar.bz2 dist-nss
