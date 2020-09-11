#!/bin/bash
set -x -e -v

source ${GECKO_PATH}/taskcluster/scripts/misc/wr-macos-cross-build-setup.sh

# The osmesa-src build which we do as part of the headless build below
# doesn't seem to always use CFLAGS/CXXFLAGS where expected. Instead we
# just squash those flags into CC/CXX and everything works out.
# Export HOST_CC and HOST_CXX without the squashed flags, so that host
# builds use them and don't see the target flags.
export HOST_CC="${CC}"
export HOST_CXX="${CXX}"
CFLAGS_VAR="CFLAGS_${TARGET_TRIPLE//-/_}"
CXXFLAGS_VAR="CXXFLAGS_${TARGET_TRIPLE//-/_}"
export CC="${CC} ${!CFLAGS_VAR}"
export ${CFLAGS_VAR}=
export CXX="${CXX} ${!CXXFLAGS_VAR}"
export ${CXXFLAGS_VAR}=

export UPLOAD_DIR="${HOME}/artifacts"
mkdir -p "${UPLOAD_DIR}"

# Do a cross-build without the `headless` feature
pushd "${GECKO_PATH}/gfx/wr/wrench"
cargo build --release -vv --frozen --target=${TARGET_TRIPLE}
# Package up the resulting wrench binary
cd "../target/${TARGET_TRIPLE}"
mkdir -p wrench-macos/bin
mv release/wrench wrench-macos/bin/
tar cjf wrench-macos.tar.bz2 wrench-macos
mv wrench-macos.tar.bz2 "${UPLOAD_DIR}"
# Clean the build
cd "${GECKO_PATH}/gfx/wr"
rm -rf target
popd

# Do a cross-build with the `headless` feature
pushd "${GECKO_PATH}/gfx/wr/wrench"
cargo build --release -vv --frozen --target=${TARGET_TRIPLE} --features headless
# Package up the wrench binary and some libraries that we will need
cd "../target/${TARGET_TRIPLE}"
mkdir wrench-macos-headless
mv release wrench-macos-headless/
tar cjf wrench-macos-headless.tar.bz2 \
    wrench-macos-headless/release/wrench \
    wrench-macos-headless/release/build/osmesa-src*/out/src/gallium/targets/osmesa/.libs \
    wrench-macos-headless/release/build/osmesa-src*/out/src/mapi/shared-glapi/.libs
mv wrench-macos-headless.tar.bz2 "${UPLOAD_DIR}"
# Clean the build
cd "${GECKO_PATH}/gfx/wr"
rm -rf target
popd
