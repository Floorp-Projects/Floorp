#!/bin/bash
set -x -e -v

export TARGET_TRIPLE="x86_64-apple-darwin"

MACOS_SYSROOT="${MOZ_FETCHES_DIR}/MacOSX14.2.sdk"
CLANGDIR="${MOZ_FETCHES_DIR}/clang"

# Deploy the wrench dependencies
mv ${MOZ_FETCHES_DIR}/wrench-deps/{vendor,.cargo} "${GECKO_PATH}/gfx/wr/"

# Building wrench with the `headless` feature also builds the osmesa-src crate,
# which includes building C++ code. We have to do a bunch of shenanigans
# to make this cross-compile properly.

pushd "${MOZ_FETCHES_DIR}/clang/bin"

# Add a pkg-config cross-compile wrapper. Without this, the configure script
# will use pkg-config from the host, which will find host libraries that are
# not what we want. This script stolen from
# https://autotools.io/pkgconfig/cross-compiling.html
cat > ${TARGET_TRIPLE}-pkg-config <<END_PKGCONFIG_WRAPPER
#!/bin/sh
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=${MACOS_SYSROOT}/usr/lib/pkgconfig:${MACOS_SYSROOT}/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${MACOS_SYSROOT}
exec pkg-config "\$@"
END_PKGCONFIG_WRAPPER
chmod +x "${TARGET_TRIPLE}-pkg-config"
popd

[ -d "${MOZ_FETCHES_DIR}/clang-mac/clang" ] && cat > ${MOZ_FETCHES_DIR}/clang-mac/clang/bin/llvm-config <<EOF_LLVM_CONFIG
#!/bin/sh
${MOZ_FETCHES_DIR}/clang/bin/llvm-config "\$@" | sed 's,${MOZ_FETCHES_DIR}/clang,${MOZ_FETCHES_DIR}/clang-mac/clang,g;s,-lLLVM-[0-9]\+,-lLLVM,g'
EOF_LLVM_CONFIG

export PATH="${MOZ_FETCHES_DIR}/rustc/bin:${MOZ_FETCHES_DIR}/clang/bin:${MOZ_FETCHES_DIR}/wrench-deps/meson:${PATH}"

# Tell the configure script where to find zlib, because otherwise it tries
# to use pkg-config to find it, which fails (no .pc file in the macos SDK).
export ZLIB_CFLAGS="-I${MACOS_SYSROOT}/usr/include"
export ZLIB_LIBS="-L${MACOS_SYSROOT}/usr/lib -lz"

# Set up compiler and flags for cross-compile. Careful to only export the
# target-specific CFLAGS/CXXFLAGS variables, to not break any host builds.
export CC="${CLANGDIR}/bin/clang"
TARGET_CFLAGS="-fuse-ld=lld -target ${TARGET_TRIPLE} -mmacosx-version-min=10.12 --rtlib=compiler-rt --sysroot ${MACOS_SYSROOT} -Qunused-arguments"
export CFLAGS_${TARGET_TRIPLE//-/_}="${TARGET_CFLAGS}"
export CXX="${CLANGDIR}/bin/clang++"
TARGET_CXXFLAGS="-fuse-ld=lld -target ${TARGET_TRIPLE} -mmacosx-version-min=10.12 --rtlib=compiler-rt --sysroot ${MACOS_SYSROOT} -stdlib=libc++ -Qunused-arguments"
export CXXFLAGS_${TARGET_TRIPLE//-/_}="${TARGET_CXXFLAGS}"
export AR="${CLANGDIR}/bin/llvm-ar"

# See documentation in cargo-linker for why we need this. TL;DR is that passing
# the right arguments to the linker when invoked by cargo is nigh impossible
# without this.
export MOZ_CARGO_WRAP_LD="${CC}"
export MOZ_CARGO_WRAP_LD_CXX="${CXX}"
export MOZ_CARGO_WRAP_LDFLAGS="${TARGET_CFLAGS}"
export CARGO_TARGET_X86_64_APPLE_DARWIN_LINKER="${GECKO_PATH}/build/cargo-linker"
