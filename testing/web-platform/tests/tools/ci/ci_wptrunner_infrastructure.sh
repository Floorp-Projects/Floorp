#!/bin/bash
set -ex

SCRIPT_DIR=$(dirname $(readlink -f "$0"))
WPT_ROOT=$(readlink -f $SCRIPT_DIR/../..)
cd $WPT_ROOT

source tools/ci/lib.sh

test_infrastructure() {
    local ARGS="";
    if [ $PRODUCT == "firefox" ]; then
        ARGS="--install-browser"
    fi
    ./wpt run --yes --manifest ~/meta/MANIFEST.json --metadata infrastructure/metadata/ --install-fonts $ARGS $PRODUCT infrastructure/
}

main() {
    hosts_fixup
    PRODUCTS=( "firefox" "chrome" )
    for PRODUCT in "${PRODUCTS[@]}"; do
        if [ $(echo $PRODUCT | grep '^chrome') ]; then
            install_chrome dev
        fi
        test_infrastructure
    done
}

main
