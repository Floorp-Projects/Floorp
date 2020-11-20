#!/bin/sh

set -e

[ "$1" != "aarch64-apple-darwin" ] && echo $1 is not supported yet && exit 1

mkdir compiler-rt
cd compiler-rt

cmake \
  $MOZ_FETCHES_DIR/llvm-project/compiler-rt \
  -GNinja \
  -DCMAKE_C_COMPILER=$MOZ_FETCHES_DIR/clang/bin/clang \
  -DCMAKE_CXX_COMPILER=$MOZ_FETCHES_DIR/clang/bin/clang++ \
  -DCMAKE_LINKER=$MOZ_FETCHES_DIR/cctools/bin/aarch64-apple-darwin-ld \
  -DCMAKE_LIPO=$MOZ_FETCHES_DIR/cctools/bin/lipo \
  -DCMAKE_AR=$MOZ_FETCHES_DIR/cctools/bin/aarch64-apple-darwin-ar \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DLLVM_CONFIG_PATH=$MOZ_FETCHES_DIR/clang/bin/llvm-config \
  -DCMAKE_C_FLAGS="-target aarch64-apple-darwin -mcpu=apple-a12 -B $MOZ_FETCHES_DIR/cctools/bin -isysroot $MOZ_FETCHES_DIR/MacOSX11.0.sdk" \
  -DCMAKE_CXX_FLAGS="-target aarch64-apple-darwin -mcpu=apple-a12 -B $MOZ_FETCHES_DIR/cctools/bin -isysroot $MOZ_FETCHES_DIR/MacOSX11.0.sdk" \
  -DCMAKE_SYSTEM_NAME=Darwin \
  -DCMAKE_SYSTEM_VERSION=11.0 \
  -DDARWIN_osx_ARCHS=arm64 \
  -DDARWIN_osx_SYSROOT=$MOZ_FETCHES_DIR/MacOSX11.0.sdk \
  -DLLVM_DEFAULT_TARGET_TRIPLE=aarch64-apple-darwin \
  -DDARWIN_macosx_OVERRIDE_SDK_VERSION=11.0 \
  -DDARWIN_osx_BUILTIN_ARCHS=arm64

# compiler-rt build script expects to find `codesign` in $PATH.
# Give it a fake one.
echo "#!/bin/sh" > codesign
chmod +x codesign

PATH=$PATH:$PWD ninja

cd ..

tar -cf - compiler-rt/lib/darwin | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > "compiler-rt.tar.zst"

mkdir -p "$UPLOAD_DIR"
mv "compiler-rt.tar.zst" "$UPLOAD_DIR"
