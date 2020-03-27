#!/bin/bash
set -e -v -x

cd $MOZ_FETCHES_DIR/winchecksec

export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"

export LD_PRELOAD=$MOZ_FETCHES_DIR/liblowercase/liblowercase.so
export LOWERCASE_DIRS=$MOZ_FETCHES_DIR/vs2017_15.8.4

. $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
. $GECKO_PATH/taskcluster/scripts/misc/tooltool-download.sh

# Patch pe-parse because clang-cl doesn't support /analyze.
patch -p1 <<'EOF'
--- a/pe-parse/cmake/compilation_flags.cmake
+++ b/pe-parse/cmake/compilation_flags.cmake
@@ -1,5 +1,5 @@
 if (MSVC)
-  list(APPEND DEFAULT_CXX_FLAGS /W4 /analyze)
+  list(APPEND DEFAULT_CXX_FLAGS /W4)

   if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
     list(APPEND DEFAULT_CXX_FLAGS /Zi)
EOF

cmake \
  -GNinja \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_C_COMPILER=clang-cl \
  -DCMAKE_LINKER=lld-link \
  -DCMAKE_C_FLAGS=-fuse-ld=lld \
  -DCMAKE_CXX_FLAGS="-fuse-ld=lld \
  -EHsc" \
  -DCMAKE_RC_COMPILER=llvm-rc \
  -DCMAKE_MT=llvm-mt \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

ninja -v

cd ..
tar -caf winchecksec.tar.bz2 winchecksec/winchecksec.{dll,exe}
cp winchecksec.tar.bz2 $UPLOAD_DIR/
