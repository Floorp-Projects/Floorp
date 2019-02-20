#!/bin/bash
set -e -v

# This is shared code for building GN.
: GN_REV                 ${GN_REV:=1ab6fa2cab7ec64840db720a56018ca8939329f9}


git clone --no-checkout https://gn.googlesource.com/gn $WORKSPACE/gn-standalone
cd $WORKSPACE/gn-standalone
git checkout $GN_REV

# Remove /MACHINE:x64 because we just let the PATH decide what cl and link are
# used, and if cl is targetting x86, we don't want linkage to fail because of
# /MACHINE:x64.
patch -p1 <<'EOF'
diff --git a/build/gen.py b/build/gen.py
index a7142fab..78d0fd56 100755
--- a/build/gen.py
+++ b/build/gen.py
@@ -373,7 +373,7 @@ def WriteGNNinja(path, platform, host, options):
         '/D_HAS_EXCEPTIONS=0',
     ])
 
-    ldflags.extend(['/DEBUG', '/MACHINE:x64'])
+    ldflags.extend(['/DEBUG'])
 
   static_libraries = {
       'base': {'sources': [
EOF

if test -n "$MAC_CROSS"; then
    python build/gen.py --platform darwin
else
    python build/gen.py
fi

ninja -C out -v

STAGE=gn
mkdir -p $UPLOAD_DIR $STAGE

# At this point, the resulting binary is at:
# $WORKSPACE/out/Release/gn
if test "$MAC_CROSS" = "" -a "$(uname)" = "Linux"; then
    strip out/gn
fi
cp out/gn $STAGE

tar -acf gn.tar.$COMPRESS_EXT $STAGE
cp gn.tar.$COMPRESS_EXT $UPLOAD_DIR
