#!/bin/bash
set -x -e -v

source ${GECKO_PATH}/taskcluster/scripts/misc/wr-macos-cross-build-setup.sh

export UPLOAD_DIR="${HOME}/artifacts"
mkdir -p "${UPLOAD_DIR}"

# Do a cross-build for cargo test run
pushd "${GECKO_PATH}/gfx/wr"
CARGOFLAGS="-vv --frozen --target=${TARGET_TRIPLE}" \
    CARGOTESTFLAGS="--no-run" \
    ci-scripts/macos-debug-tests.sh
# Package up the test binaries
cd "target/${TARGET_TRIPLE}"
mkdir cargo-test-binaries
mv debug cargo-test-binaries/
find cargo-test-binaries/debug/deps -type f -maxdepth 1 -executable -print0 > binaries.lst
tar cjf cargo-test-binaries.tar.bz2 --null -T binaries.lst
mv cargo-test-binaries.tar.bz2 "${UPLOAD_DIR}"
# Clean the build
cd "${GECKO_PATH}/gfx/wr"
rm -rf target
popd
