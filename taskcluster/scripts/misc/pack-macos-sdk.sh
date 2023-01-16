#!/bin/bash

set -x
set -e
set -o pipefail

PKG=$1
SHA256=$2
SDK_DIR=$3

curl -OL $PKG
shasum -a 256 -c <<EOF
$SHA256  $(basename $PKG)
EOF
$GECKO_PATH/mach python $(dirname $0)/unpack-sdk.py $(basename $PKG) pkg/
mv pkg/$SDK_DIR $(basename $SDK_DIR)

$(dirname $0)/pack.sh $(basename $SDK_DIR)
