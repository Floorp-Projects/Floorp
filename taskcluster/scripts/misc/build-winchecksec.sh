#!/bin/bash
set -e -v -x

mkdir -p $UPLOAD_DIR

cd $MOZ_FETCHES_DIR/winchecksec

SUFFIX=

case "$1" in
x86_64-pc-windows-msvc)
    SUFFIX=.exe
    export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

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

    CMAKE_FLAGS='
      -DCMAKE_CXX_COMPILER=clang-cl
      -DCMAKE_C_COMPILER=clang-cl
      -DCMAKE_LINKER=lld-link
      -DCMAKE_C_FLAGS="-fuse-ld=lld -Xclang -ivfsoverlay -Xclang $MOZ_FETCHES_DIR/vs/overlay.yaml"
      -DCMAKE_CXX_FLAGS="-fuse-ld=lld -EHsc -Xclang -ivfsoverlay -Xclang $MOZ_FETCHES_DIR/vs/overlay.yaml"
      -DCMAKE_RC_COMPILER=llvm-rc
      -DCMAKE_MT=llvm-mt
      -DCMAKE_SYSTEM_NAME=Windows
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
    '
    ;;
esac

eval cmake \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=Off \
  $CMAKE_FLAGS

ninja -v

cd ..
tar -caf winchecksec.tar.zst winchecksec/winchecksec${SUFFIX}
cp winchecksec.tar.zst $UPLOAD_DIR/
