#!/bin/bash
set -x -e -v

# This script is for building clang for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

cd $HOME_DIR/src

. taskcluster/scripts/misc/tooltool-download.sh

git clone -n https://github.com/llvm-mirror/llvm

cd llvm
git checkout 4727bc748a48e46824eae55a81ae890cd25c3a34

patch -p1 <<'EOF'
diff --git a/lib/DebugInfo/DWARF/DWARFDie.cpp b/lib/DebugInfo/DWARF/DWARFDie.cpp
index 17559d2..b08a8d9 100644
--- a/lib/DebugInfo/DWARF/DWARFDie.cpp
+++ b/lib/DebugInfo/DWARF/DWARFDie.cpp
@@ -304,20 +304,24 @@ DWARFDie::find(ArrayRef<dwarf::Attribute> Attrs) const {
 
 Optional<DWARFFormValue>
 DWARFDie::findRecursively(ArrayRef<dwarf::Attribute> Attrs) const {
   if (!isValid())
     return None;
   if (auto Value = find(Attrs))
     return Value;
   if (auto Die = getAttributeValueAsReferencedDie(DW_AT_abstract_origin)) {
+    if (Die.getOffset() == getOffset())
+      return None;
     if (auto Value = Die.findRecursively(Attrs))
       return Value;
   }
   if (auto Die = getAttributeValueAsReferencedDie(DW_AT_specification)) {
+    if (Die.getOffset() == getOffset())
+      return None;
     if (auto Value = Die.findRecursively(Attrs))
       return Value;
   }
   return None;
 }
 
 DWARFDie
 DWARFDie::getAttributeValueAsReferencedDie(dwarf::Attribute Attr) const {
EOF

mkdir build
cd build

cmake \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DCMAKE_C_COMPILER=$HOME_DIR/src/gcc/bin/gcc \
  -DCMAKE_CXX_COMPILER=$HOME_DIR/src/gcc/bin/g++ \
  -DCMAKE_C_FLAGS="-B $HOME_DIR/src/gcc/bin" \
  -DCMAKE_CXX_FLAGS="-B $HOME_DIR/src/gcc/bin" \
  ..

export LD_LIBRARY_PATH=$HOME_DIR/src/gcc/lib64

ninja llvm-dsymutil llvm-symbolizer

tar --xform='s,^,llvm-dsymutil/,' -Jcf llvm-dsymutil.tar.xz bin/llvm-dsymutil bin/llvm-symbolizer

mkdir -p $UPLOAD_DIR
cp llvm-dsymutil.tar.xz $UPLOAD_DIR
