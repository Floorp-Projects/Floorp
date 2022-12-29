#!/bin/bash
set -x -e -v

. $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

cd $MOZ_FETCHES_DIR/make

# Patch for http://savannah.gnu.org/bugs/?58656
patch -p1 <<'EOF'
diff --git a/src/remake.c b/src/remake.c
index fb237c5..b2ba069 100644
--- a/src/remake.c
+++ b/src/remake.c
@@ -35,6 +35,13 @@ this program.  If not, see <http://www.gnu.org/licenses/>.  */
 #endif
 #ifdef WINDOWS32
 #include <io.h>
+#include <sys/stat.h>
+#if defined(_MSC_VER) && _MSC_VER > 1200
+/* VC7 or later support _stat64 to access 64-bit file size. */
+#define stat64 _stat64
+#else
+#define stat64 stat
+#endif
 #endif


@@ -1466,7 +1473,11 @@ static FILE_TIMESTAMP
 name_mtime (const char *name)
 {
   FILE_TIMESTAMP mtime;
+#if defined(WINDOWS32)
+  struct stat64 st;
+#else
   struct stat st;
+#endif
   int e;

 #if defined(WINDOWS32)
@@ -1498,7 +1509,7 @@ name_mtime (const char *name)
         tend = &tem[0];
       }

-    e = stat (tem, &st);
+    e = stat64 (tem, &st);
     if (e == 0 && !_S_ISDIR (st.st_mode) && tend < tem + (p - name - 1))
       {
         errno = ENOTDIR;
EOF

chmod +w src/config.h.W32
sed "/#define BATCH_MODE_ONLY_SHELL/s/\/\*\(.*\)\*\//\1/" src/config.h.W32 > src/config.h
make -f Basic.mk \
  MAKE_HOST=Windows32 \
  MKDIR.cmd='mkdir -p $1' \
  RM.cmd='rm -f $1' \
  CP.cmd='cp $1 $2' \
  msvc_CC="$MOZ_FETCHES_DIR/clang/bin/clang-cl -Xclang -ivfsoverlay -Xclang $MOZ_FETCHES_DIR/vs/overlay.yaml" \
  msvc_LD=$MOZ_FETCHES_DIR/clang/bin/lld-link

mkdir mozmake
cp WinRel/gnumake.exe mozmake/mozmake.exe

tar -acvf mozmake.tar.zst mozmake
mkdir -p $UPLOAD_DIR
cp mozmake.tar.zst $UPLOAD_DIR
