#!/bin/bash
set -x -e -v

# This script is for building a mingw-clang toolchain for use on Linux.

if [[ $# -eq 0 ]]; then
    echo "Provide either x86 or x64 to specify a toolchain."
    exit 1;
elif [ "$1" == "x86" ]; then
  machine="i686"
  compiler_rt_machine="i386"
  crt_flags="--enable-lib32 --disable-lib64"
  WRAPPER_FLAGS="-fsjlj-exceptions"
elif [ "$1" == "x64" ]; then
  machine="x86_64"
  compiler_rt_machine="x86_64"
  crt_flags="--disable-lib32 --enable-lib64"
  WRAPPER_FLAGS=""
else
  echo "Provide either x86 or x64 to specify a toolchain."
  exit 1;
fi

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

TOOLCHAIN_DIR=$WORKSPACE/moz-toolchain
INSTALL_DIR=$TOOLCHAIN_DIR/build/stage3/clang
CROSS_PREFIX_DIR=$INSTALL_DIR/$machine-w64-mingw32
SRC_DIR=$TOOLCHAIN_DIR/src

make_flags="-j$(nproc)"

mingw_version=164990461720e0ed6ea5ea9f359d78595b1a459a
libunwind_version=6ee92fcc97350ae32db3172a269e9afcc2bab686
llvm_mingw_version=c3a16814bd26aa6702e1e5b482a3d9044bb0f725

# This is default value of _WIN32_WINNT. Gecko configure script explicitly sets this,
# so this is not used to build Gecko itself. We default to 0x601, which is Windows 7.
default_win32_winnt=0x601

cd $HOME_DIR/src

. taskcluster/scripts/misc/tooltool-download.sh
patch_file="$(pwd)/taskcluster/scripts/misc/mingw-winrt.patch"

prepare() {
  mkdir -p $TOOLCHAIN_DIR
  touch $TOOLCHAIN_DIR/.build-clang

  mkdir -p $SRC_DIR
  pushd $SRC_DIR

  git clone -n git://git.code.sf.net/p/mingw-w64/mingw-w64
  pushd mingw-w64
  git checkout $mingw_version
  patch -p1 <$patch_file
  popd

  git clone https://github.com/llvm-mirror/libunwind.git
  pushd libunwind
  git checkout $libunwind_version
  popd

  git clone https://github.com/mstorsjo/llvm-mingw.git
  pushd llvm-mingw
  git checkout $llvm_mingw_version
  popd

  popd
}

install_wrappers() {
  pushd $INSTALL_DIR/bin

  compiler_flags="--sysroot \$DIR/../$machine-w64-mingw32 -rtlib=compiler-rt -stdlib=libc++ -fuse-ld=lld $WRAPPER_FLAGS -fuse-cxa-atexit -Qunused-arguments"

  cat <<EOF >$machine-w64-mingw32-clang
#!/bin/sh
DIR="\$(cd "\$(dirname "\$0")" && pwd)"
\$DIR/clang -target $machine-w64-mingw32 $compiler_flags "\$@"
EOF
  chmod +x $machine-w64-mingw32-clang

  cat <<EOF >$machine-w64-mingw32-clang++
#!/bin/sh
DIR="\$(cd "\$(dirname "\$0")" && pwd)"
\$DIR/clang -target $machine-w64-mingw32 --driver-mode=g++ $compiler_flags "\$@"
EOF
  chmod +x $machine-w64-mingw32-clang++

  CC="$machine-w64-mingw32-clang"
  CXX="$machine-w64-mingw32-clang++"

  popd
}

build_mingw() {
  mkdir mingw-w64-headers
  pushd mingw-w64-headers
  $SRC_DIR/mingw-w64/mingw-w64-headers/configure --host=$machine-w64-mingw32 \
                                                 --enable-sdk=all \
                                                 --enable-idl \
                                                 --with-default-msvcrt=ucrt \
                                                 --with-default-win32-winnt=$default_win32_winnt \
                                                 --prefix=$CROSS_PREFIX_DIR
  make $make_flags install
  popd

  mkdir mingw-w64-crt
  pushd mingw-w64-crt
  $SRC_DIR/mingw-w64/mingw-w64-crt/configure --host=$machine-w64-mingw32 \
                                             $crt_flags \
                                             --with-default-msvcrt=ucrt \
                                             CC="$CC" \
                                             AR=llvm-ar \
                                             RANLIB=llvm-ranlib \
                                             DLLTOOL=llvm-dlltool \
                                             --prefix=$CROSS_PREFIX_DIR
  make $make_flags
  make $make_flags install
  popd

  mkdir widl
  pushd widl
  $SRC_DIR/mingw-w64/mingw-w64-tools/widl/configure --target=$machine-w64-mingw32 --prefix=$INSTALL_DIR
  make $make_flags
  make $make_flags install
  popd
}

build_compiler_rt() {
  CLANG_VERSION=$(basename $(dirname $(dirname $(dirname $($CC --print-libgcc-file-name -rtlib=compiler-rt)))))
  mkdir compiler-rt
  pushd compiler-rt
  cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=$CC \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_AR=$INSTALL_DIR/bin/llvm-ar \
      -DCMAKE_RANLIB=$INSTALL_DIR/bin/llvm-ranlib \
      -DCMAKE_C_COMPILER_WORKS=1 \
      -DCMAKE_C_COMPILER_TARGET=$compiler_rt_machine-windows-gnu \
      -DCOMPILER_RT_DEFAULT_TARGET_ONLY=TRUE \
      $SRC_DIR/compiler-rt/lib/builtins
  make $make_flags
  mkdir -p $INSTALL_DIR/lib/clang/$CLANG_VERSION/lib/windows
  cp lib/windows/libclang_rt.builtins-$compiler_rt_machine.a $INSTALL_DIR/lib/clang/$CLANG_VERSION/lib/windows/
  popd
}

merge_libs() {
  cat <<EOF |llvm-ar -M
CREATE tmp.a
ADDLIB $1
ADDLIB $2
SAVE
END
EOF
  llvm-ranlib tmp.a
  mv tmp.a $1
}

build_libcxx() {
  # Below, we specify -g -gcodeview to build static libraries with debug information.
  # Because we're not distributing these builds, this is fine. If one were to distribute
  # the builds, perhaps one would want to make those flags conditional or investigation
  # other options.
  DEBUG_FLAGS="-g -gcodeview"

  mkdir libunwind
  pushd libunwind
  cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$CROSS_PREFIX_DIR \
      -DCMAKE_C_COMPILER=$CC \
      -DCMAKE_CXX_COMPILER=$CXX \
      -DCMAKE_CROSSCOMPILING=TRUE \
      -DCMAKE_SYSROOT=$CROSS_PREFIX_DIR \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER_WORKS=TRUE \
      -DCMAKE_CXX_COMPILER_WORKS=TRUE \
      -DLLVM_COMPILER_CHECKED=TRUE \
      -DCMAKE_AR=$INSTALL_DIR/bin/llvm-ar \
      -DCMAKE_RANLIB=$INSTALL_DIR/bin/llvm-ranlib \
      -DLLVM_NO_OLD_LIBSTDCXX=TRUE \
      -DCXX_SUPPORTS_CXX11=TRUE \
      -DCXX_SUPPORTS_CXX_STD=TRUE \
      -DLIBUNWIND_USE_COMPILER_RT=TRUE \
      -DLIBUNWIND_ENABLE_THREADS=TRUE \
      -DLIBUNWIND_ENABLE_SHARED=FALSE \
      -DLIBUNWIND_ENABLE_CROSS_UNWINDING=FALSE \
      -DCMAKE_CXX_FLAGS="${DEBUG_FLAGS} -Wno-dll-attribute-on-redeclaration -nostdinc++ -I$SRC_DIR/libcxx/include -DPSAPI_VERSION=2" \
      -DCMAKE_C_FLAGS="-Wno-dll-attribute-on-redeclaration" \
      $SRC_DIR/libunwind
  make $make_flags
  make $make_flags install
  popd

  mkdir libcxxabi
  pushd libcxxabi
  cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$CROSS_PREFIX_DIR \
      -DCMAKE_C_COMPILER=$CC \
      -DCMAKE_CXX_COMPILER=$CXX \
      -DCMAKE_CROSSCOMPILING=TRUE \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER_WORKS=TRUE \
      -DCMAKE_CXX_COMPILER_WORKS=TRUE \
      -DCMAKE_SYSROOT=$CROSS_PREFIX_DIR \
      -DLLVM_COMPILER_CHECKED=TRUE \
      -DCMAKE_AR=$INSTALL_DIR/bin/llvm-ar \
      -DCMAKE_RANLIB=$INSTALL_DIR/bin/llvm-ranlib \
      -DLIBCXXABI_USE_COMPILER_RT=ON \
      -DLIBCXXABI_ENABLE_EXCEPTIONS=ON \
      -DLIBCXXABI_ENABLE_THREADS=ON \
      -DLIBCXXABI_TARGET_TRIPLE=$machine-w64-mingw32 \
      -DLIBCXXABI_ENABLE_SHARED=OFF \
      -DLIBCXXABI_LIBCXX_INCLUDES=$SRC_DIR/libcxx/include \
      -DLLVM_NO_OLD_LIBSTDCXX=TRUE \
      -DCXX_SUPPORTS_CXX11=TRUE \
      -DCXX_SUPPORTS_CXX_STD=TRUE \
      -DCMAKE_CXX_FLAGS="${DEBUG_FLAGS} -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS -D_LIBCPP_HAS_THREAD_API_WIN32" \
      $SRC_DIR/libcxxabi
  make $make_flags VERBOSE=1
  popd

  mkdir libcxx
  pushd libcxx
  cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$CROSS_PREFIX_DIR \
      -DCMAKE_C_COMPILER=$CC \
      -DCMAKE_CXX_COMPILER=$CXX \
      -DCMAKE_CROSSCOMPILING=TRUE \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER_WORKS=TRUE \
      -DCMAKE_CXX_COMPILER_WORKS=TRUE \
      -DLLVM_COMPILER_CHECKED=TRUE \
      -DCMAKE_AR=$INSTALL_DIR/bin/llvm-ar \
      -DCMAKE_RANLIB=$INSTALL_DIR/bin/llvm-ranlib \
      -DLIBCXX_USE_COMPILER_RT=ON \
      -DLIBCXX_INSTALL_HEADERS=ON \
      -DLIBCXX_ENABLE_EXCEPTIONS=ON \
      -DLIBCXX_ENABLE_THREADS=ON \
      -DLIBCXX_HAS_WIN32_THREAD_API=ON \
      -DLIBCXX_ENABLE_MONOTONIC_CLOCK=ON \
      -DLIBCXX_ENABLE_SHARED=OFF \
      -DLIBCXX_SUPPORTS_STD_EQ_CXX11_FLAG=TRUE \
      -DLIBCXX_HAVE_CXX_ATOMICS_WITHOUT_LIB=TRUE \
      -DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=OFF \
      -DLIBCXX_ENABLE_FILESYSTEM=OFF \
      -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=TRUE \
      -DLIBCXX_CXX_ABI=libcxxabi \
      -DLIBCXX_CXX_ABI_INCLUDE_PATHS=$SRC_DIR/libcxxabi/include \
      -DLIBCXX_CXX_ABI_LIBRARY_PATH=../libcxxabi/lib \
      -DCMAKE_CXX_FLAGS="${DEBUG_FLAGS} -D_LIBCXXABI_DISABLE_VISIBILITY_ANNOTATIONS" \
      $SRC_DIR/libcxx
  make $make_flags VERBOSE=1
  make $make_flags install

  # libc++.a depends on libunwind.a. Whild linker will automatically link
  # to libc++.a in C++ mode, it won't pick libunwind.a, requiring caller
  # to explicitly pass -lunwind. Wo work around that, we merge libunwind.a
  # into libc++.a.
  merge_libs $CROSS_PREFIX_DIR/lib/libc++.a $CROSS_PREFIX_DIR/lib/libunwind.a
  popd
}

build_utils() {
  pushd $INSTALL_DIR/bin/
  ln -s llvm-nm $machine-w64-mingw32-nm
  ln -s llvm-strip $machine-w64-mingw32-strip
  ln -s llvm-readobj $machine-w64-mingw32-readobj
  ln -s llvm-objcopy $machine-w64-mingw32-objcopy
  ./clang $SRC_DIR/llvm-mingw/wrappers/windres-wrapper.c -O2 -Wl,-s -o $machine-w64-mingw32-windres
  popd
}

export PATH=$INSTALL_DIR/bin:$PATH

prepare

# gets a bit too verbose here
set +x

cd build/build-clang
# |mach python| sets up a virtualenv for us!
../../mach python ./build-clang.py -c clang-8-mingw.json --skip-tar

set -x

pushd $TOOLCHAIN_DIR/build

install_wrappers
build_mingw
build_compiler_rt
build_libcxx
build_utils

popd

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR

pushd $(dirname $INSTALL_DIR)
tar caf clangmingw.tar.xz clang
mv clangmingw.tar.xz $UPLOAD_DIR
popd
