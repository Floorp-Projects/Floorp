#!/usr/bin/env bash

set -v -e -x

# Set up the toolchain.
if [[ "$@" == *"-m32"* ]]; then
  source $(dirname $0)/setup32.sh
else
  source $(dirname $0)/setup64.sh
fi

# Install GYP.
cd gyp
python -m virtualenv test-env
test-env/Scripts/python setup.py install
test-env/Scripts/python -m pip install --upgrade pip
test-env/Scripts/pip install --upgrade setuptools
cd ..

export GYP_MSVS_OVERRIDE_PATH="${VSPATH}"
export GYP_MSVS_VERSION="2015"
export GYP="${PWD}/gyp/test-env/Scripts/gyp"

# Fool GYP.
touch "${VSPATH}/VC/vcvarsall.bat"

# Clone NSPR.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build with gyp.
GYP=${GYP} ./nss/build.sh -g -v "$@"

# Package.
7z a public/build/dist.7z dist
