#!/bin/bash
set -e -v

# This script is for building tup on Linux.

COMPRESS_EXT=xz
export PATH=$MOZ_FETCHES_DIR/gcc/bin:$PATH

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

cd $MOZ_FETCHES_DIR/tup

patch -p1 <<'EOF'
diff --git a/src/tup/link.sh b/src/tup/link.sh
index ed9a01c6..4ecc6d7d 100755
--- a/src/tup/link.sh
+++ b/src/tup/link.sh
@@ -4,7 +4,7 @@
 # linking, so that the version is updated whenever we change anything that
 # affects the tup binary. This used to live in the Tupfile, but to support
 # Windows local builds we need to make it an explicit shell script.
-version=`git describe`
+version=unknown
 CC=$1
 CFLAGS=$2
 LDFLAGS=$3
EOF

(echo 'CONFIG_TUP_SERVER=ldpreload'; echo 'CONFIG_TUP_USE_SYSTEM_PCRE=n') > tup.config
./bootstrap-ldpreload.sh
cd ..
tar caf tup.tar.xz tup/tup tup/tup-ldpreload.so tup/tup.1
cp tup.tar.xz $UPLOAD_DIR
