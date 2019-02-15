#!/bin/bash
set -x -e -v

export WORKSPACE="/builds/worker/workspace"
export TOOLS_DIR="${WORKSPACE}/build/src"
export TARGET_TRIPLE="x86_64-apple-darwin"

mkdir -p "${TOOLS_DIR}"

# The tooltool-download script assumes that the gecko checkout is in
# ${WORKSPACE}/build/src and that it can run `./mach` there. This is not
# true when using run-task (which is what this script runs with) so we
# symlink mach to satisfy tooltool-download's assumptions.
ln -s "${GECKO_PATH}/mach" "${TOOLS_DIR}"
source "${GECKO_PATH}/taskcluster/scripts/misc/tooltool-download.sh"

MACOS_SYSROOT="${TOOLS_DIR}/MacOSX10.11.sdk"
CLANGDIR="${TOOLS_DIR}/clang"

# Deploy the wrench dependencies
mv ${TOOLS_DIR}/wrench-deps/{vendor,.cargo} "${GECKO_PATH}/gfx/wr/"

# Building wrench with the `headless` feature also builds the osmesa-src crate,
# which includes building C++ code. We have to do a bunch of shenanigans
# to make this cross-compile properly.

pushd "${TOOLS_DIR}/cctools/bin"
# Link all the tools from x86_64-darwin11-* to x86_64-apple-darwin-* because
# x86_64-apple-darwin is the Official Rust Triple (TM) and so that's the prefix
# we run configure with, and the toolchain configure/make will look for.
# We can't just rename the files because e.g. the ar tool invokes the ranlib
# tool with the old prefix hard-coded. So we symlink instead.
for TOOL in x86_64-darwin11-*; do
    ln -s "${TOOL}" "${TOOL/x86_64-darwin11/${TARGET_TRIPLE}}"
done

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
export PATH="${TOOLS_DIR}/rustc/bin:${TOOLS_DIR}/cctools/bin:${TOOLS_DIR}/llvm-dsymutil/bin:${PATH}"

# The x86_64-darwin11-ld linker from cctools requires libraries provided
# by clang, so we need to set LD_LIBRARY_PATH for that to work.
export LD_LIBRARY_PATH="${CLANGDIR}/lib:${LD_LIBRARY_PATH}"

# Tell the configure script where to find zlib, because otherwise it tries
# to use pkg-config to find it, which fails (no .pc file in the macos SDK).
export ZLIB_CFLAGS="-I${MACOS_SYSROOT}/usr/include"
export ZLIB_LIBS="-L${MACOS_SYSROOT}/usr/lib -lz"

# Set up compiler and flags for cross-compile
LDPATH="${TOOLS_DIR}/cctools/bin/${TARGET_TRIPLE}-ld"
export CC="${CLANGDIR}/bin/clang"
export CFLAGS="-fuse-ld=${LDPATH} -target ${TARGET_TRIPLE} -mmacosx-version-min=10.7 --rtlib=compiler-rt --sysroot ${MACOS_SYSROOT}"
export CXX="${CLANGDIR}/bin/clang++"
export CXXFLAGS="-fuse-ld=${LDPATH} -target ${TARGET_TRIPLE} -mmacosx-version-min=10.7 --rtlib=compiler-rt --sysroot ${MACOS_SYSROOT} -stdlib=libc++"

# See documentation in cargo-linker for why we need this. TL;DR is that passing
# the right arguments to the linker when invoked by cargo is nigh impossible
# without this.
export MOZ_CARGO_WRAP_LD="${CC}"
export MOZ_CARGO_WRAP_LDFLAGS="${CFLAGS}"
export CARGO_TARGET_X86_64_APPLE_DARWIN_LINKER="${GECKO_PATH}/build/cargo-linker"
