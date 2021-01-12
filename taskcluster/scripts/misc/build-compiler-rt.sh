#!/bin/sh

set -e

target=$1

case "$target" in
aarch64-apple-darwin)
  arch=arm64
  sdk=11.0
  extra_flags="-mcpu=apple-a12"
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

compiler_wrapper() {
cat > $1 <<EOF
exec \$MOZ_FETCHES_DIR/clang/bin/$1 -target $target $extra_flags -isysroot \$MOZ_FETCHES_DIR/MacOSX$sdk.sdk "\$@"
EOF
chmod +x $1
}
compiler_wrapper clang
compiler_wrapper clang++

patch -d $MOZ_FETCHES_DIR/llvm-project -p1 < $GECKO_PATH/build/build-clang/rename_gcov_flush_clang_11.patch

cmake \
  $MOZ_FETCHES_DIR/llvm-project/compiler-rt \
  -GNinja \
  -DCMAKE_C_COMPILER=$PWD/clang \
  -DCMAKE_CXX_COMPILER=$PWD/clang++ \
  -DCMAKE_LINKER=$MOZ_FETCHES_DIR/cctools/bin/$target-ld \
  -DCMAKE_LIPO=$MOZ_FETCHES_DIR/cctools/bin/lipo \
  -DCMAKE_AR=$MOZ_FETCHES_DIR/cctools/bin/$target-ar \
  -DCMAKE_RANLIB=$MOZ_FETCHES_DIR/cctools/bin/$target-ranlib \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DLLVM_CONFIG_PATH=$MOZ_FETCHES_DIR/clang/bin/llvm-config \
  -DCMAKE_SYSTEM_NAME=Darwin \
  -DCMAKE_SYSTEM_VERSION=$sdk \
  -DDARWIN_osx_ARCHS=$arch \
  -DDARWIN_osx_SYSROOT=$MOZ_FETCHES_DIR/MacOSX$sdk.sdk \
  -DLLVM_DEFAULT_TARGET_TRIPLE=$target \
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
