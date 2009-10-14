# Usage: cp $1/update.sh <liboggz_src_directory>
#
# Copies the needed files from a directory containing the original
# liboggz source that we need for the Mozilla HTML5 media support.
cp $1/config.h ./include/oggz/config_gcc.h
cp $1/win32/config.h ./include/oggz/config_win32.h
cat >./include/oggz/config.h <<EOF
#if defined(WIN32) && !defined(__GNUC__)
#include "config_win32.h"
#else
#include "config_gcc.h"
#endif
#undef DEBUG
EOF
cp $1/include/oggz/oggz_write.h ./include/oggz/oggz_write.h
cp $1/include/oggz/oggz_io.h ./include/oggz/oggz_io.h
cp $1/include/oggz/oggz_seek.h ./include/oggz/oggz_seek.h
cp $1/include/oggz/oggz_comments.h ./include/oggz/oggz_comments.h
cp $1/include/oggz/oggz_read.h ./include/oggz/oggz_read.h
cp $1/include/oggz/oggz_off_t.h ./include/oggz/oggz_off_t.h
cp $1/include/oggz/oggz_table.h ./include/oggz/oggz_table.h
cp $1/include/oggz/oggz.h ./include/oggz/oggz.h
cp $1/include/oggz/oggz_constants.h ./include/oggz/oggz_constants.h
cp $1/include/oggz/oggz_deprecated.h ./include/oggz/oggz_deprecated.h
cp $1/include/oggz/oggz_stream.h ./include/oggz/oggz_stream.h
cp $1/COPYING ./COPYING
cp $1/README ./README
cp $1/ChangeLog ./ChangeLog
cp $1/src/liboggz/oggz_write.c ./src/liboggz/oggz_write.c
cp $1/src/liboggz/oggz_table.c ./src/liboggz/oggz_table.c
cp $1/src/liboggz/oggz_dlist.c ./src/liboggz/oggz_dlist.c
cp $1/src/liboggz/oggz_auto.c ./src/liboggz/oggz_auto.c
cp $1/src/liboggz/oggz_private.h ./src/liboggz/oggz_private.h
cp $1/src/liboggz/oggz.c ./src/liboggz/oggz.c
cp $1/src/liboggz/oggz_compat.h ./src/liboggz/oggz_compat.h
cp $1/src/liboggz/oggz_read.c ./src/liboggz/oggz_read.c
cp $1/src/liboggz/oggz_macros.h ./src/liboggz/oggz_macros.h
cp $1/src/liboggz/oggz_comments.c ./src/liboggz/oggz_comments.c
cp $1/src/liboggz/oggz_byteorder.h ./src/liboggz/oggz_byteorder.h
cp $1/src/liboggz/oggz_stream.c ./src/liboggz/oggz_stream.c
cp $1/src/liboggz/oggz_stream_private.h ./src/liboggz/oggz_stream_private.h
cp $1/src/liboggz/oggz_vector.h ./src/liboggz/oggz_vector.h
cp $1/src/liboggz/oggz_auto.h ./src/liboggz/oggz_auto.h
cp $1/src/liboggz/oggz_io.c ./src/liboggz/oggz_io.c
cp $1/src/liboggz/oggz_vector.c ./src/liboggz/oggz_vector.c
cp $1/src/liboggz/oggz_seek.c ./src/liboggz/oggz_seek.c
cp $1/src/liboggz/oggz_dlist.h ./src/liboggz/oggz_dlist.h
cp $1/src/liboggz/metric_internal.c ./src/liboggz/metric_internal.c
cp $1/src/liboggz/dirac.h ./src/liboggz/dirac.h
cp $1/src/liboggz/dirac.c ./src/liboggz/dirac.c
cp $1/AUTHORS ./AUTHORS
patch -p3 <wince.patch
patch -p3 <endian.patch
patch -p3 <key_frame_seek.patch
patch -p3 <offset_next.patch
patch -p3 <bug487519.patch
patch -p3 <bug496063.patch
patch -p3 <faster_seek.patch
patch -p3 <bug516847.patch
patch -p3 <bug518169.patch
patch -p3 <bug504843.patch
patch -p3 <bug519155.patch
patch -p3 <bug520493.patch
