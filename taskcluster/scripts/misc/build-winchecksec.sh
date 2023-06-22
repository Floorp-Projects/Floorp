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

# Apply https://github.com/trailofbits/pe-parse/commit/d9e72af81e832330c111e07b98d34877469445f5
# And https://github.com/trailofbits/pe-parse/commit/eecdb3d36eb44e306398a2e66e85490f9bdcc74c
patch -p1 <<'EOF'
--- a/pe-parse/pe-parser-library/src/buffer.cpp
+++ b/pe-parse/pe-parser-library/src/buffer.cpp
@@ -112,11 +112,12 @@ bool readWord(bounded_buffer *b, std::uint32_t offset, std::uint16_t &out) {
     return false;
   }
 
-  std::uint16_t *tmp = reinterpret_cast<std::uint16_t *>(b->buf + offset);
+  std::uint16_t tmp;
+  memcpy(&tmp, (b->buf + offset), sizeof(std::uint16_t));
   if (b->swapBytes) {
-    out = byteSwapUint16(*tmp);
+    out = byteSwapUint16(tmp);
   } else {
-    out = *tmp;
+    out = tmp;
   }
 
   return true;
@@ -133,11 +134,12 @@ bool readDword(bounded_buffer *b, std::uint32_t offset, std::uint32_t &out) {
     return false;
   }
 
-  std::uint32_t *tmp = reinterpret_cast<std::uint32_t *>(b->buf + offset);
+  std::uint32_t tmp;
+  memcpy(&tmp, (b->buf + offset), sizeof(std::uint32_t));
   if (b->swapBytes) {
-    out = byteSwapUint32(*tmp);
+    out = byteSwapUint32(tmp);
   } else {
-    out = *tmp;
+    out = tmp;
   }
 
   return true;
@@ -154,11 +156,12 @@ bool readQword(bounded_buffer *b, std::uint32_t offset, std::uint64_t &out) {
     return false;
   }
 
-  std::uint64_t *tmp = reinterpret_cast<std::uint64_t *>(b->buf + offset);
+  std::uint64_t tmp;
+  memcpy(&tmp, (b->buf + offset), sizeof(std::uint64_t));
   if (b->swapBytes) {
-    out = byteSwapUint64(*tmp);
+    out = byteSwapUint64(tmp);
   } else {
-    out = *tmp;
+    out = tmp;
   }
 
   return true;
@@ -175,16 +178,16 @@ bool readChar16(bounded_buffer *b, std::uint32_t offset, char16_t &out) {
     return false;
   }
 
-  char16_t *tmp = nullptr;
+  char16_t tmp;
   if (b->swapBytes) {
     std::uint8_t tmpBuf[2];
     tmpBuf[0] = *(b->buf + offset + 1);
     tmpBuf[1] = *(b->buf + offset);
-    tmp = reinterpret_cast<char16_t *>(tmpBuf);
+    memcpy(&tmp, tmpBuf, sizeof(std::uint16_t));
   } else {
-    tmp = reinterpret_cast<char16_t *>(b->buf + offset);
+    memcpy(&tmp, (b->buf + offset), sizeof(std::uint16_t));
   }
-  out = *tmp;
+  out = tmp;
 
   return true;
 }
--- a/pe-parse/pe-parser-library/include/parser-library/parse.h
+++ b/pe-parse/pe-parser-library/include/parser-library/parse.h
@@ -40,28 +40,38 @@ THE SOFTWARE.
   err_loc.assign(__func__);     \
   err_loc += ":" + to_string<std::uint32_t>(__LINE__, std::dec);
 
-#define READ_WORD(b, o, inst, member)                                     \
-  if (!readWord(b, o + _offset(__typeof__(inst), member), inst.member)) { \
-    PE_ERR(PEERR_READ);                                                   \
-    return false;                                                         \
+#define READ_WORD(b, o, inst, member)                                          \
+  if (!readWord(b,                                                             \
+                o + static_cast<uint32_t>(offsetof(__typeof__(inst), member)), \
+                inst.member)) {                                                \
+    PE_ERR(PEERR_READ);                                                        \
+    return false;                                                              \
   }
 
-#define READ_DWORD(b, o, inst, member)                                     \
-  if (!readDword(b, o + _offset(__typeof__(inst), member), inst.member)) { \
-    PE_ERR(PEERR_READ);                                                    \
-    return false;                                                          \
+#define READ_DWORD(b, o, inst, member)                                   \
+  if (!readDword(                                                        \
+          b,                                                             \
+          o + static_cast<uint32_t>(offsetof(__typeof__(inst), member)), \
+          inst.member)) {                                                \
+    PE_ERR(PEERR_READ);                                                  \
+    return false;                                                        \
   }
 
-#define READ_QWORD(b, o, inst, member)                                     \
-  if (!readQword(b, o + _offset(__typeof__(inst), member), inst.member)) { \
-    PE_ERR(PEERR_READ);                                                    \
-    return false;                                                          \
+#define READ_QWORD(b, o, inst, member)                                   \
+  if (!readQword(                                                        \
+          b,                                                             \
+          o + static_cast<uint32_t>(offsetof(__typeof__(inst), member)), \
+          inst.member)) {                                                \
+    PE_ERR(PEERR_READ);                                                  \
+    return false;                                                        \
   }
 
-#define READ_BYTE(b, o, inst, member)                                     \
-  if (!readByte(b, o + _offset(__typeof__(inst), member), inst.member)) { \
-    PE_ERR(PEERR_READ);                                                   \
-    return false;                                                         \
+#define READ_BYTE(b, o, inst, member)                                          \
+  if (!readByte(b,                                                             \
+                o + static_cast<uint32_t>(offsetof(__typeof__(inst), member)), \
+                inst.member)) {                                                \
+    PE_ERR(PEERR_READ);                                                        \
+    return false;                                                              \
   }
 
 #define TEST_MACHINE_CHARACTERISTICS(h, m, ch) \
--- a/pe-parse/pe-parser-library/src/parse.cpp
+++ b/pe-parse/pe-parser-library/src/parse.cpp
@@ -1777,7 +1777,7 @@ bool getRelocations(parsed_pe *p) {
         // Mask out the type and assign
         type = entry >> 12;
         // Mask out the offset and assign
-        offset = entry & ~0xf000;
+        offset = entry & static_cast<std::uint16_t>(~0xf000);
 
         // Produce the VA of the relocation
         VA relocVA;
EOF

eval cmake \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=Off \
  $CMAKE_FLAGS

ninja -v

cd ..
tar -caf winchecksec.tar.zst winchecksec/winchecksec${SUFFIX}
cp winchecksec.tar.zst $UPLOAD_DIR/
