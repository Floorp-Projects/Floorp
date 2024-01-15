#!/bin/bash
set -x -e -v

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
project=${artifact%.tar.*}
workspace=$HOME/workspace

# Exported for osx-cross-linker.
export TARGET=$1
shift

FEATURES="$@"

case "$TARGET" in
x86_64-unknown-linux-gnu)
    # Native Linux Build
    export RUSTFLAGS="-Clinker=$MOZ_FETCHES_DIR/clang/bin/clang++ -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu -C link-arg=-fuse-ld=lld"
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
    export TARGET_CFLAGS="--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    export TARGET_CXXFLAGS="-D_GLIBCXX_USE_CXX11_ABI=0 --sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    ;;
*-apple-darwin)
    # Cross-compiling for Mac on Linux.
    export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"
    export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
    if test "$TARGET" = "aarch64-apple-darwin"; then
        export MACOSX_DEPLOYMENT_TARGET=11.0
    else
        export MACOSX_DEPLOYMENT_TARGET=10.12
    fi
    export CC="$MOZ_FETCHES_DIR/clang/bin/clang"
    export CXX="$MOZ_FETCHES_DIR/clang/bin/clang++"
    export TARGET_CFLAGS="-isysroot $MOZ_FETCHES_DIR/MacOSX14.2.sdk"
    export TARGET_CXXFLAGS="-isysroot $MOZ_FETCHES_DIR/MacOSX14.2.sdk -stdlib=libc++"
    ;;
*-pc-windows-msvc)
    # Cross-compiling for Windows on Linux.
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang-cl
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang-cl
    export TARGET_AR=$MOZ_FETCHES_DIR/clang/bin/llvm-lib

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    export CARGO_TARGET_I686_PC_WINDOWS_MSVC_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
    export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
    export TARGET_CFLAGS="-Xclang -ivfsoverlay -Xclang $MOZ_FETCHES_DIR/vs/overlay.yaml"
    export TARGET_CXXFLAGS="-Xclang -ivfsoverlay -Xclang $MOZ_FETCHES_DIR/vs/overlay.yaml"
    ;;
esac

PATH="$MOZ_FETCHES_DIR/rustc/bin:$PATH"

CRATE_PATH=$MOZ_FETCHES_DIR/${FETCH-$project}
WORKSPACE_ROOT=$(cd $CRATE_PATH; cargo metadata --format-version 1 --no-deps --locked 2> /dev/null | jq -r .workspace_root)

if test ! -f $WORKSPACE_ROOT/Cargo.lock; then
  CARGO_LOCK=taskcluster/scripts/misc/$project-Cargo.lock
  if test -f $GECKO_PATH/$CARGO_LOCK; then
    cp $GECKO_PATH/$CARGO_LOCK $WORKSPACE_ROOT/Cargo.lock
  else
    echo "Missing Cargo.lock for the crate. Please provide one in $CARGO_LOCK" >&2
    exit 1
  fi
fi

cargo install \
  --locked \
  --verbose \
  --path $CRATE_PATH \
  --target-dir $workspace/obj \
  --root $workspace/out \
  --target "$TARGET" \
  ${FEATURES:+--features "$FEATURES"}

mkdir $workspace/$project
mv $workspace/out/bin/* $workspace/$project
tar -C $workspace -acvf $project.tar.zst $project
mkdir -p $UPLOAD_DIR
mv $project.tar.zst $UPLOAD_DIR
