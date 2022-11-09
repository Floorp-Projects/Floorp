#!/bin/bash
set -x -e -v

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}

cd $MOZ_FETCHES_DIR/wasi-sdk
LLVM_PROJ_DIR=$MOZ_FETCHES_DIR/llvm-project

# Apply patch from https://github.com/WebAssembly/wasi-libc/pull/344.
patch -p1 <<'EOF'
diff --git a/src/wasi-libc/Makefile b/src/wasi-libc/Makefile
index 44acdee..6e01bed 100644
--- a/src/wasi-libc/Makefile
+++ b/src/wasi-libc/Makefile
@@ -556,6 +556,8 @@ check-symbols: startup_files libc
 	    -U__clang_version__ \
 	    -U__clang_literal_encoding__ \
 	    -U__clang_wide_literal_encoding__ \
+	    -U__wasm_mutable_globals__ \
+	    -U__wasm_sign_ext__ \
 	    -U__GNUC__ \
 	    -U__GNUC_MINOR__ \
 	    -U__GNUC_PATCHLEVEL__ \
@@ -565,6 +567,7 @@ check-symbols: startup_files libc
 	    -U__BITINT_MAXWIDTH__ \
 	    -U__FLT_EVAL_METHOD__ -Wno-builtin-macro-redefined \
 	    | sed -e 's/__[[:upper:][:digit:]]*_ATOMIC_\([[:upper:][:digit:]_]*\)_LOCK_FREE/__compiler_ATOMIC_\1_LOCK_FREE/' \
+	    | sed -e 's/__GNUC_VA_LIST $$/__GNUC_VA_LIST 1/' \
 	    | grep -v '^#define __FLT16_' \
 	    | grep -v '^#define __\(BOOL\|INT_\(LEAST\|FAST\)\(8\|16\|32\|64\)\|INT\|LONG\|LLONG\|SHRT\)_WIDTH__' \
 	    > "$(SYSROOT_SHARE)/predefined-macros.txt"
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
# `BULK_MEMORY_SOURCES=` force-disables building things with -mbulk-memory,
# which wasm2c doesn't support yet.
make \
  LLVM_PROJ_DIR=$LLVM_PROJ_DIR \
  BULK_MEMORY_SOURCES= \
  PREFIX=/wasi \
  build/compiler-rt.BUILT \
  -j$(nproc)

mkdir -p $dir/lib
mv build/install/wasi/lib/clang/*/lib/wasi $dir/lib
tar --zstd -cf $artifact $dir
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR/
