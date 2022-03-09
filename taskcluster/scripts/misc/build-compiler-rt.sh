#!/bin/sh

set -e -x

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}
target=${dir#compiler-rt-}

clang=$MOZ_FETCHES_DIR/clang/bin/clang

case "$target" in
aarch64-apple-darwin)
  arch=arm64
  export MACOSX_DEPLOYMENT_TARGET=11.0
  compiler_wrapper() {
    echo exec \$MOZ_FETCHES_DIR/clang/bin/$1 -mcpu=apple-a12 \"\$@\" > $1
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
    -DCMAKE_LINKER=$MOZ_FETCHES_DIR/clang/bin/ld64.lld
    -DCMAKE_LIPO=$MOZ_FETCHES_DIR/clang/bin/llvm-lipo
    -DCMAKE_SYSTEM_NAME=Darwin
    -DCMAKE_SYSTEM_VERSION=$MACOSX_DEPLOYMENT_TARGET
    -DCMAKE_OSX_SYSROOT=$MOZ_FETCHES_DIR/MacOSX11.0.sdk
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
    -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld
    -DDARWIN_osx_ARCHS=$arch
    -DDARWIN_osx_SYSROOT=$MOZ_FETCHES_DIR/MacOSX11.0.sdk
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
    -DCOMPILER_RT_BUILD_LIBFUZZER=OFF
    -DCOMPILER_RT_BUILD_ORC=OFF
  "
  ;;
*-unknown-linux-gnu)
  EXTRA_CMAKE_FLAGS="
    -DCMAKE_SYSROOT=$MOZ_FETCHES_DIR/sysroot-${target%-unknown-linux-gnu}-linux-gnu
    -DCMAKE_LINKER=$MOZ_FETCHES_DIR/clang/bin/ld.lld
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
    -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld
  "
  ;;
*-pc-windows-msvc)
  VSPATH="$MOZ_FETCHES_DIR/vs2017_15.9.6"
  SDK_VERSION=10.0.17134.0

  export INCLUDE="${VSPATH}/VC/include;${VSPATH}/VC/atlmfc/include;${VSPATH}/SDK/Include/${SDK_VERSION}/ucrt;${VSPATH}/SDK/Include/${SDK_VERSION}/shared;${VSPATH}/SDK/Include/${SDK_VERSION}/um"
  case "$target" in
  i686-pc-windows-msvc)
    VCARCH=x86
    ;;
  x86_64-pc-windows-msvc)
    VCARCH=x64
    ;;
  aarch64-pc-windows-msvc)
    VCARCH=arm64
    ;;
  esac
  export LIB="${VSPATH}/VC/lib/${VCARCH};${VSPATH}/VC/atlmfc/lib/${VCARCH};${VSPATH}/SDK/Lib/${SDK_VERSION}/um/${VCARCH};${VSPATH}/SDK/Lib/${SDK_VERSION}/ucrt/${VCARCH}"
  export LD_PRELOAD=$MOZ_FETCHES_DIR/liblowercase/liblowercase.so
  export LOWERCASE_DIRS=$VSPATH
  clang=$MOZ_FETCHES_DIR/clang/bin/clang-cl
  clangxx=$clang
  ar=lib
  EXTRA_CMAKE_FLAGS="
    -DCMAKE_SYSTEM_NAME=Windows
    -DCMAKE_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
    -DCMAKE_MT=$MOZ_FETCHES_DIR/clang/bin/llvm-mt
    -DCMAKE_RC_COMPILER=$MOZ_FETCHES_DIR/clang/bin/llvm-rc
    -DCMAKE_C_FLAGS='--target=$target -fms-compatibility-version=19.15.26726'
    -DCMAKE_CXX_FLAGS='--target=$target -fms-compatibility-version=19.15.26726'
    -DCMAKE_ASM_FLAGS=--target=$target
  "
  ;;
*)
  echo $target is not supported yet
  exit 1
  ;;
esac

if [ -n "$TOOLTOOL_MANIFEST" ]; then
  . $GECKO_PATH/taskcluster/scripts/misc/tooltool-download.sh
fi

mkdir compiler-rt
cd compiler-rt

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
  $MOZ_FETCHES_DIR/llvm-project/compiler-rt \
  -GNinja \
  -DCMAKE_C_COMPILER=$clang \
  -DCMAKE_CXX_COMPILER=${clangxx:-$clang++} \
  -DCMAKE_C_COMPILER_TARGET=$target \
  -DCMAKE_CXX_COMPILER_TARGET=$target \
  -DCMAKE_ASM_COMPILER_TARGET=$target \
  -DCMAKE_AR=$MOZ_FETCHES_DIR/clang/bin/llvm-${ar:-ar} \
  -DCMAKE_RANLIB=$MOZ_FETCHES_DIR/clang/bin/llvm-ranlib \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=${PWD}/${dir} \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DLLVM_CONFIG_PATH=$MOZ_FETCHES_DIR/clang/bin/llvm-config \
  -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
  $EXTRA_CMAKE_FLAGS

ninja install -v

# ninja install doesn't copy the PDBs
case "$target" in
*-pc-windows-msvc)
  cp lib/windows/*pdb $dir/lib/windows/
  ;;
esac

tar caf "$artifact" "$dir"

mkdir -p "$UPLOAD_DIR"
mv "$artifact" "$UPLOAD_DIR"
