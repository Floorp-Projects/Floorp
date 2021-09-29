#!/bin/bash
set -x -e -v

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}

cd $MOZ_FETCHES_DIR/wasi-sdk
LLVM_PROJ_DIR=$MOZ_FETCHES_DIR/llvm-project

# https://github.com/WebAssembly/wasi-sdk/pull/189
patch -p1 <<'EOF'
diff --git a/Makefile b/Makefile
index bde9936..b1f24fe 100644
--- a/Makefile
+++ b/Makefile
@@ -91,7 +91,7 @@ build/wasi-libc.BUILT: build/llvm.BUILT
 		SYSROOT=$(BUILD_PREFIX)/share/wasi-sysroot
 	touch build/wasi-libc.BUILT
 
-build/compiler-rt.BUILT: build/llvm.BUILT
+build/compiler-rt.BUILT: build/llvm.BUILT build/wasi-libc.BUILT
 	# Do the build, and install it.
 	mkdir -p build/compiler-rt
 	cd build/compiler-rt && cmake -G Ninja \
EOF

# Build compiler-rt
make \
  LLVM_PROJ_DIR=$LLVM_PROJ_DIR \
  PREFIX=/wasi \
  build/compiler-rt.BUILT \
  -j$(nproc)

mkdir -p $dir/lib
mv build/install/wasi/lib/clang/*/lib/wasi $dir/lib
tar --zstd -cf $artifact $dir
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR/
