#!/bin/bash -vex

set -x -e

# Inputs, with defaults

: REPOSITORY   ${REPOSITORY:=https://github.com/rust-lang/cargo}
: BRANCH       ${BRANCH:=master}

: WORKSPACE    ${WORKSPACE:=/home/worker}

set -v

# Check out rust sources
SRCDIR=${WORKSPACE}/cargo
git clone --recursive $REPOSITORY -b $BRANCH ${SRCDIR}

# Report version
VERSION=$(git -C ${SRCDIR} describe --tags --dirty)
COMMIT=$(git -C ${SRCDIR} rev-parse HEAD)
echo "cargo ${VERSION} (commit ${COMMIT})" | tee cargo-version
