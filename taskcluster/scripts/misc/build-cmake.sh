#!/bin/bash
set -e -v -x

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
project=${artifact%.tar.*}
workspace=$HOME/workspace

cd $MOZ_FETCHES_DIR/cmake

# Work around https://gitlab.kitware.com/cmake/cmake/-/issues/26031
patch -p1 <<'EOF'
diff --git a/Source/bindexplib.cxx b/Source/bindexplib.cxx
index 52e200c24f..07ccf3965c 100644
--- a/Source/bindexplib.cxx
+++ b/Source/bindexplib.cxx
@@ -398,7 +398,7 @@ static bool DumpFile(std::string const& nmPath, const char* filename,
                      std::set<std::string>& symbols,
                      std::set<std::string>& dataSymbols)
 {
-#ifndef _WIN32
+#if 1
   return DumpFileWithLlvmNm(nmPath, filename, symbols, dataSymbols);
 #else
   HANDLE hFile;
EOF

# Work around https://github.com/llvm/llvm-project/issues/94563
# The resulting cmake works well enough for our use.
patch -p1 <<'EOF'
diff --git a/Source/CMakeLists.txt b/Source/CMakeLists.txt
index c268a92111..d18f8cf221 100644
--- a/Source/CMakeLists.txt
+++ b/Source/CMakeLists.txt
@@ -863,7 +863,6 @@ if(WIN32)
 
     # Add a manifest file to executables on Windows to allow for
     # GetVersion to work properly on Windows 8 and above.
-    target_sources(ManifestLib INTERFACE cmake.version.manifest)
   endif()
 endif()
 
EOF

export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"

cmake \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang-cl \
  -DCMAKE_CXX_COMPILER=clang-cl \
  -DCMAKE_LINKER=lld-link \
  -DCMAKE_MT=llvm-mt \
  -DCMAKE_C_FLAGS="-Xclang -ivfsoverlay -Xclang $MOZ_FETCHES_DIR/vs/overlay.yaml -winsysroot $MOZ_FETCHES_DIR/vs" \
  -DCMAKE_CXX_FLAGS="-GR -EHsc -Xclang -ivfsoverlay -Xclang $MOZ_FETCHES_DIR/vs/overlay.yaml -winsysroot $MOZ_FETCHES_DIR/vs" \
  -DCMAKE_EXE_LINKER_FLAGS="-winsysroot:$MOZ_FETCHES_DIR/vs" \
  -DCMAKE_MODULE_LINKER_FLAGS="-winsysroot:$MOZ_FETCHES_DIR/vs" \
  -DCMAKE_SHARED_LINKER_FLAGS="-winsysroot:$MOZ_FETCHES_DIR/vs" \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded \
  -DCMAKE_INSTALL_PREFIX=$workspace/$project \
  -B $workspace/build

ninja -C $workspace/build -v install

tar -C $workspace -acvf $artifact $project
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR
