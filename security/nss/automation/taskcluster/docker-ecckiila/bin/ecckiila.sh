#!/usr/bin/env bash

set -v -e -x

if [ $(id -u) = 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker $0
fi

git clone --depth=1 https://gitlab.com/nisec/ecckiila.git
