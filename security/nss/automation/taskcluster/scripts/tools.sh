#!/usr/bin/env bash

set -v -e -x

switch_compilers() {
    GCC=`which ${GCC_VERSION:-gcc-5}`
    GXX=`which ${GXX_VERSION:-g++-5}`

    if [ -e "$GCC" ] && [ -e "$GXX" ]; then
        update-alternatives --set gcc $GCC
        update-alternatives --set g++ $GXX
    else
        echo "Unknown compiler $GCC_VERSION/$GXX_VERSION."
        exit 1
    fi
}
