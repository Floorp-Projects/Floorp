#!/bin/bash
set -x -e -v

cd $MOZ_FETCHES_DIR/msix-packaging

export PATH=$MOZ_FETCHES_DIR/clang/bin:$PATH

# makelinux.sh invokes `make` with no parallelism.  These jobs run on hosts with
# 16+ vCPUs; let's try to take advantage.
export MAKEFLAGS=-j16

./makelinux.sh --pack -- \
      -DCMAKE_SYSROOT=$MOZ_FETCHES_DIR/sysroot \
      -DCMAKE_EXE_LINKER_FLAGS_INIT='-fuse-ld=lld -Wl,-rpath=\$ORIGIN' \
      -DCMAKE_SHARED_LINKER_FLAGS_INIT='-fuse-ld=lld -Wl,-rpath=\$ORIGIN' \
      -DCMAKE_SKIP_BUILD_RPATH=TRUE

mkdir msix-packaging
cp .vs/bin/makemsix msix-packaging
cp .vs/lib/libmsix.so msix-packaging

# The `msix-packaging` tool links against libicu dynamically.  It would be
# better to link statically, but it's not easy to achieve.  This copies the
# needed libicu libraries from the sysroot, and the rpath settings above allows
# them to be loaded, which means the consuming environment doesn't need to
# install libicu directly.
LD_LIBRARY_PATH=$MOZ_FETCHES_DIR/sysroot/usr/lib/x86_64-linux-gnu \
ldd msix-packaging/libmsix.so | awk '$3 ~ /libicu/ {print $3}' | xargs -I '{}' cp '{}' msix-packaging

tar caf msix-packaging.tar.zst msix-packaging

mkdir -p $UPLOAD_DIR
cp msix-packaging.tar.zst $UPLOAD_DIR
