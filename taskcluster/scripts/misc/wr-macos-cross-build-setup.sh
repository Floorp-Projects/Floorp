#!/bin/bash
set -x -e -v

export TARGET_TRIPLE="x86_64-apple-darwin"

source "${GECKO_PATH}/taskcluster/scripts/misc/tooltool-download.sh"

MACOS_SYSROOT="${MOZ_FETCHES_DIR}/MacOSX10.11.sdk"
CLANGDIR="${MOZ_FETCHES_DIR}/clang"

# Deploy the wrench dependencies
mv ${MOZ_FETCHES_DIR}/wrench-deps/{vendor,.cargo} "${GECKO_PATH}/gfx/wr/"

# Building wrench with the `headless` feature also builds the osmesa-src crate,
# which includes building C++ code. We have to do a bunch of shenanigans
# to make this cross-compile properly.

pushd "${MOZ_FETCHES_DIR}/cctools/bin"

# Add a pkg-config cross-compile wrapper. Without this, the configure script
# will use pkg-config from the host, which will find host libraries that are
# not what we want. This script stolen from
# https://autotools.io/pkgconfig/cross-compiling.html
cat > ${TARGET_TRIPLE}-pkg-config <<END_PKGCONFIG_WRAPPER
#!/bin/sh
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=${MACOS_SYSROOT}/usr/lib/pkgconfig:${MACOS_SYSROOT}/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${MACOS_SYSROOT}
exec pkg-config "$@"
END_PKGCONFIG_WRAPPER
chmod +x "${TARGET_TRIPLE}-pkg-config"
popd

# The PATH intentionally excludes clang/bin because osmesa will try to
# build llvmpipe if it finds a llvm-config. And that will fail because
# we don't have a target libLLVM library to link with. We don't need
# llvmpipe anyway since we only use the softpipe driver. If for whatever
# reason we need to add clang/bin to the path here, we should be able to
# instead set LLVM_CONFIG=no to disable llvmpipe, but that might impact
# other parts of the build.
export PATH="${MOZ_FETCHES_DIR}/rustc/bin:${MOZ_FETCHES_DIR}/cctools/bin:${MOZ_FETCHES_DIR}/llvm-dsymutil/bin:${PATH}"

# The x86_64-darwin11-ld linker from cctools requires libraries provided
# by clang, so we need to set LD_LIBRARY_PATH for that to work.
export LD_LIBRARY_PATH="${CLANGDIR}/lib:${LD_LIBRARY_PATH}"

# Tell the configure script where to find zlib, because otherwise it tries
# to use pkg-config to find it, which fails (no .pc file in the macos SDK).
export ZLIB_CFLAGS="-I${MACOS_SYSROOT}/usr/include"
export ZLIB_LIBS="-L${MACOS_SYSROOT}/usr/lib -lz"

# Set up compiler and flags for cross-compile. Careful to only export the
# target-specific CFLAGS/CXXFLAGS variables, to not break any host builds.
LDPATH="${MOZ_FETCHES_DIR}/cctools/bin/${TARGET_TRIPLE}-ld"
export CC="${CLANGDIR}/bin/clang"
TARGET_CFLAGS="-fuse-ld=${LDPATH} -target ${TARGET_TRIPLE} -mmacosx-version-min=10.7 --rtlib=compiler-rt --sysroot ${MACOS_SYSROOT}"
export CFLAGS_${TARGET_TRIPLE//-/_}="${TARGET_CFLAGS}"
export CXX="${CLANGDIR}/bin/clang++"
TARGET_CXXFLAGS="-fuse-ld=${LDPATH} -target ${TARGET_TRIPLE} -mmacosx-version-min=10.7 --rtlib=compiler-rt --sysroot ${MACOS_SYSROOT} -stdlib=libc++"
export CXXFLAGS_${TARGET_TRIPLE//-/_}="${TARGET_CXXFLAGS}"
export AR="${CLANGDIR}/bin/llvm-ar"

# See documentation in cargo-linker for why we need this. TL;DR is that passing
# the right arguments to the linker when invoked by cargo is nigh impossible
# without this.
export MOZ_CARGO_WRAP_LD="${CC}"
export MOZ_CARGO_WRAP_LDFLAGS="${TARGET_CFLAGS}"
export CARGO_TARGET_X86_64_APPLE_DARWIN_LINKER="${GECKO_PATH}/build/cargo-linker"
