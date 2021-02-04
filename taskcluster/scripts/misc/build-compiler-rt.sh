#!/bin/sh

set -e

target=$1
shift

clang=$MOZ_FETCHES_DIR/clang/bin/clang

case "$target" in
aarch64-apple-darwin)
  arch=arm64
  sdk=11.0
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
  sdk=10.12
  ;;
*)
  echo $target is not supported yet
  exit 1
  ;;
esac

export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"

if [ -n "$TOOLTOOL_MANIFEST" ]; then
  . $GECKO_PATH/taskcluster/scripts/misc/tooltool-download.sh
fi

mkdir compiler-rt
cd compiler-rt

for patchfile in "$@"; do
  patch -d $MOZ_FETCHES_DIR/llvm-project -p1 < $GECKO_PATH/$patchfile
done

cmake \
  $MOZ_FETCHES_DIR/llvm-project/compiler-rt \
  -GNinja \
  -DCMAKE_C_COMPILER=$clang \
  -DCMAKE_CXX_COMPILER=$clang++ \
  -DCMAKE_C_COMPILER_TARGET=$target \
  -DCMAKE_CXX_COMPILER_TARGET=$target \
  -DCMAKE_ASM_COMPILER_TARGET=$target \
  -DCMAKE_AR=$MOZ_FETCHES_DIR/clang/bin/llvm-ar \
  -DCMAKE_RANLIB=$MOZ_FETCHES_DIR/clang/bin/llvm-ranlib \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DLLVM_CONFIG_PATH=$MOZ_FETCHES_DIR/clang/bin/llvm-config \
  -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
  -DCMAKE_LINKER=$MOZ_FETCHES_DIR/cctools/bin/$target-ld \
  -DCMAKE_LIPO=$MOZ_FETCHES_DIR/cctools/bin/lipo \
  -DCMAKE_SYSTEM_NAME=Darwin \
  -DCMAKE_SYSTEM_VERSION=$sdk \
  -DCMAKE_OSX_SYSROOT=$MOZ_FETCHES_DIR/MacOSX$sdk.sdk \
  -DDARWIN_osx_ARCHS=$arch \
  -DDARWIN_osx_SYSROOT=$MOZ_FETCHES_DIR/MacOSX$sdk.sdk \
  -DDARWIN_macosx_OVERRIDE_SDK_VERSION=$sdk \
  -DDARWIN_osx_BUILTIN_ARCHS=$arch

# compiler-rt build script expects to find `codesign` in $PATH.
# Give it a fake one.
echo "#!/bin/sh" > codesign
chmod +x codesign

PATH=$PATH:$PWD ninja -v

cd ..

tar -cf - compiler-rt/lib/darwin | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > "compiler-rt.tar.zst"

mkdir -p "$UPLOAD_DIR"
mv "compiler-rt.tar.zst" "$UPLOAD_DIR"
