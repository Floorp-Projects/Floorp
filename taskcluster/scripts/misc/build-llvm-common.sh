#!/bin/sh

set -e -x

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}
what=$1
shift
install=$1
shift
target=$1
shift

clang=$MOZ_FETCHES_DIR/clang/bin/clang

case "$target" in
aarch64-apple-darwin)
  arch=arm64
  export MACOSX_DEPLOYMENT_TARGET=11.0
  compiler_wrapper() {
    echo exec \$MOZ_FETCHES_DIR/clang/bin/$1 -mcpu=apple-m1 \"\$@\" > $1
    chmod +x $1
  }
  compiler_wrapper clang
  compiler_wrapper clang++
  clang=$PWD/clang
  ;;
x86_64-apple-darwin)
  arch=x86_64
  export MACOSX_DEPLOYMENT_TARGET=10.12
  ;;
armv7-linux-android)
  api_level=16
  ndk_target=arm-linux-androideabi
  ndk_prefix=arm-linux-androideabi
  ndk_arch=arm
  ;;
aarch64-linux-android)
  api_level=21
  ndk_target=aarch64-linux-android
  ndk_prefix=aarch64-linux-android
  ndk_arch=arm64
  ;;
i686-linux-android)
  api_level=16
  ndk_target=i686-linux-android
  ndk_prefix=x86
  ndk_arch=x86
  ;;
x86_64-linux-android)
  api_level=21
  ndk_target=x86_64-linux-android
  ndk_prefix=x86_64
  ndk_arch=x86_64
  ;;
esac

case "$target" in
*-apple-darwin)
  EXTRA_CMAKE_FLAGS="
    $EXTRA_CMAKE_FLAGS
    -DCMAKE_LINKER=$MOZ_FETCHES_DIR/clang/bin/ld64.lld
    -DCMAKE_LIPO=$MOZ_FETCHES_DIR/clang/bin/llvm-lipo
    -DCMAKE_SYSTEM_NAME=Darwin
    -DCMAKE_SYSTEM_VERSION=$MACOSX_DEPLOYMENT_TARGET
    -DCMAKE_OSX_SYSROOT=$MOZ_FETCHES_DIR/MacOSX11.3.sdk
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
    -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld
    -DDARWIN_osx_ARCHS=$arch
    -DDARWIN_osx_SYSROOT=$MOZ_FETCHES_DIR/MacOSX11.3.sdk
    -DDARWIN_macosx_OVERRIDE_SDK_VERSION=11.0
    -DDARWIN_osx_BUILTIN_ARCHS=$arch
    -DLLVM_DEFAULT_TARGET_TRIPLE=$target
  "
  # compiler-rt build script expects to find `codesign` in $PATH.
  # Give it a fake one.
  echo "#!/bin/sh" > codesign
  chmod +x codesign
  PATH="$PATH:$PWD"
  ;;
*-linux-android)
  cflags="
    --gcc-toolchain=$MOZ_FETCHES_DIR/android-ndk/toolchains/$ndk_prefix-4.9/prebuilt/linux-x86_64
    -isystem $MOZ_FETCHES_DIR/android-ndk/sysroot/usr/include/$ndk_target
    -isystem $MOZ_FETCHES_DIR/android-ndk/sysroot/usr/include
    -D__ANDROID_API__=$api_level
  "
  # These flags are only necessary to pass the cmake tests.
  exe_linker_flags="
    --rtlib=libgcc
    -L$MOZ_FETCHES_DIR/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$ndk_target/$api_level
    -L$MOZ_FETCHES_DIR/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$ndk_target
  "
  EXTRA_CMAKE_FLAGS="
    $EXTRA_CMAKE_FLAGS
    -DCMAKE_SYSROOT=$MOZ_FETCHES_DIR/android-ndk/platforms/android-$api_level/arch-$ndk_arch
    -DCMAKE_LINKER=$MOZ_FETCHES_DIR/clang/bin/ld.lld
    -DCMAKE_C_FLAGS='-fPIC $cflags'
    -DCMAKE_ASM_FLAGS='$cflags'
    -DCMAKE_CXX_FLAGS='-fPIC -Qunused-arguments $cflags'
    -DCMAKE_EXE_LINKER_FLAGS='-fuse-ld=lld $exe_linker_flags'
    -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld
    -DANDROID=1
    -DANDROID_NATIVE_API_LEVEL=$api_level
    -DSANITIZER_ALLOW_CXXABI=OFF
  "
  ;;
*-unknown-linux-gnu)
  if [ -d "$MOZ_FETCHES_DIR/sysroot" ]; then
    sysroot=$MOZ_FETCHES_DIR/sysroot
  else
    sysroot=$MOZ_FETCHES_DIR/sysroot-${target%-unknown-linux-gnu}-linux-gnu
  fi
  EXTRA_CMAKE_FLAGS="
    $EXTRA_CMAKE_FLAGS
    -DCMAKE_SYSROOT=$sysroot
    -DCMAKE_LINKER=$MOZ_FETCHES_DIR/clang/bin/ld.lld
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
    -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld
  "
  ;;
*-pc-windows-msvc)
  export LD_PRELOAD="/builds/worker/fetches/liblowercase/liblowercase.so"
  export LOWERCASE_DIRS="/builds/worker/fetches/vs"
  EXTRA_CMAKE_FLAGS="
    $EXTRA_CMAKE_FLAGS
    -DCMAKE_TOOLCHAIN_FILE=$MOZ_FETCHES_DIR/llvm-project/llvm/cmake/platforms/WinMsvc.cmake
    -DLLVM_NATIVE_TOOLCHAIN=$MOZ_FETCHES_DIR/clang
    -DHOST_ARCH=${target%-pc-windows-msvc}
    -DLLVM_DISABLE_ASSEMBLY_FILES=ON
  "
  # LLVM 15+ uses different input variables.
  if grep -q LLVM_WINSYSROOT $MOZ_FETCHES_DIR/llvm-project/llvm/cmake/platforms/WinMsvc.cmake; then
    EXTRA_CMAKE_FLAGS="
      $EXTRA_CMAKE_FLAGS
      -DLLVM_WINSYSROOT=$MOZ_FETCHES_DIR/vs
    "
  else
    # WinMsvc.cmake before LLVM 15 doesn't support spaces in WINDSK_BASE.
    ln -s "windows kits/10" $MOZ_FETCHES_DIR/vs/sdk
    EXTRA_CMAKE_FLAGS="
      $EXTRA_CMAKE_FLAGS
      -DMSVC_BASE=$MOZ_FETCHES_DIR/vs/vc/tools/msvc/14.29.30133
      -DWINSDK_BASE=$MOZ_FETCHES_DIR/vs/sdk
      -DWINSDK_VER=10.0.19041.0
    "
  fi
  ;;
*)
  echo $target is not supported yet
  exit 1
  ;;
esac

case "$target" in
*-pc-windows-msvc)
  ;;
*)
  EXTRA_CMAKE_FLAGS="
    $EXTRA_CMAKE_FLAGS
    -DCMAKE_C_COMPILER=$clang
    -DCMAKE_CXX_COMPILER=$clang++
    -DCMAKE_AR=$MOZ_FETCHES_DIR/clang/bin/llvm-ar
    -DCMAKE_RANLIB=$MOZ_FETCHES_DIR/clang/bin/llvm-ranlib
  "
  ;;
esac

mkdir build
cd build

for patchfile in "$@"; do
  case $patchfile in
  *.json)
      jq -r '.patches[]' $GECKO_PATH/$patchfile | while read p; do
        patch -d $MOZ_FETCHES_DIR/llvm-project -p1 < $GECKO_PATH/$(dirname $patchfile)/$p
      done
      ;;
  *)
      patch -d $MOZ_FETCHES_DIR/llvm-project -p1 < $GECKO_PATH/$patchfile
      ;;
  esac
done

eval cmake \
  $MOZ_FETCHES_DIR/llvm-project/$what \
  -GNinja \
  -DCMAKE_C_COMPILER_TARGET=$target \
  -DCMAKE_CXX_COMPILER_TARGET=$target \
  -DCMAKE_ASM_COMPILER_TARGET=$target \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=${PWD}/${dir} \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DLLVM_CONFIG_PATH=$MOZ_FETCHES_DIR/clang/bin/llvm-config \
  $EXTRA_CMAKE_FLAGS

ninja -v $install

if [ "$what" = "compiler-rt" ]; then
  # ninja install doesn't copy the PDBs
  case "$target" in
  *-pc-windows-msvc)
    cp lib/windows/*pdb $dir/lib/windows/
    ;;
  esac
fi

tar caf "$artifact" "$dir"

mkdir -p "$UPLOAD_DIR"
mv "$artifact" "$UPLOAD_DIR"
