#!/bin/bash
set -x -e -v

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}

cd $MOZ_FETCHES_DIR/wasi-sdk
LLVM_PROJ_DIR=$MOZ_FETCHES_DIR/llvm-project

# Apply patch from https://github.com/WebAssembly/wasi-libc/pull/265
patch -p1 <<'EOF'
diff --git a/src/wasi-libc/Makefile b/src/wasi-libc/Makefile
index f597985..1bec3ae 100644
--- a/src/wasi-libc/Makefile
+++ b/src/wasi-libc/Makefile
@@ -530,6 +530,7 @@ check-symbols: startup_files libc
 	@#
 	@# TODO: Undefine __FLOAT128__ for now since it's not in clang 8.0.
 	@# TODO: Filter out __FLT16_* for now, as not all versions of clang have these.
+	@# TODO: Filter out __NO_MATH_ERRNO_ and a few __*WIDTH__ that are new to clang 14.
 	$(WASM_CC) $(CFLAGS) "$(SYSROOT_SHARE)/include-all.c" \
 	    -isystem $(SYSROOT_INC) \
 	    -std=gnu17 \
@@ -548,8 +549,11 @@ check-symbols: startup_files libc
 	    -U__GNUC_PATCHLEVEL__ \
 	    -U__VERSION__ \
 	    -U__FLOAT128__ \
+	    -U__NO_MATH_ERRNO__ \
+	    -U__BITINT_MAXWIDTH__ \
 	    | sed -e 's/__[[:upper:][:digit:]]*_ATOMIC_\([[:upper:][:digit:]_]*\)_LOCK_FREE/__compiler_ATOMIC_\1_LOCK_FREE/' \
 	    | grep -v '^#define __FLT16_' \
+	    | grep -v '^#define __\(BOOL\|INT_\(LEAST\|FAST\)\(8\|16\|32\|64\)\|INT\|LONG\|LLONG\|SHRT\)_WIDTH__' \
 	    > "$(SYSROOT_SHARE)/predefined-macros.txt"
 
 	# Check that the computed metadata matches the expected metadata.
EOF

mkdir -p build/install/wasi
# The wasi-sdk build system wants to build clang itself. We trick it into
# thinking it did, and put our own clang where it would have built its own.
ln -s $MOZ_FETCHES_DIR/clang build/llvm
touch build/llvm.BUILT

# The wasi-sdk build system wants a clang and an ar binary in
# build/install/$PREFIX/bin
ln -s $MOZ_FETCHES_DIR/clang/bin build/install/wasi/bin
ln -s llvm-ar build/install/wasi/bin/ar

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
