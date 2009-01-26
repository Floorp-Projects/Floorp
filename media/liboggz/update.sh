# Usage: cp $1/update.sh <liboggz_src_directory>
#
# Copies the needed files from a directory containing the original
# liboggz source that we need for the Mozilla HTML5 media support.
cp $1/config.h ./include/oggz/config.h
echo "#undef DEBUG" >>./include/oggz/config.h
cp $1/win32/config.h ./include/oggz/config_win32.h
echo "#undef DEBUG" >>./include/oggz/config_win32.h
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
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_write.c >./src/liboggz/oggz_write.c
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_table.c >./src/liboggz/oggz_table.c
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_dlist.c >./src/liboggz/oggz_dlist.c
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_auto.c >./src/liboggz/oggz_auto.c
cp $1/src/liboggz/oggz_private.h ./src/liboggz/oggz_private.h
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz.c >./src/liboggz/oggz.c
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_compat.h >./src/liboggz/oggz_compat.h
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_read.c >./src/liboggz/oggz_read.c
cp $1/src/liboggz/oggz_macros.h ./src/liboggz/oggz_macros.h
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_comments.c >./src/liboggz/oggz_comments.c
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_byteorder.h >./src/liboggz/oggz_byteorder.h
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_stream.c >./src/liboggz/oggz_stream.c
cp $1/src/liboggz/oggz_stream_private.h ./src/liboggz/oggz_stream_private.h
cp $1/src/liboggz/oggz_vector.h ./src/liboggz/oggz_vector.h
cp $1/src/liboggz/oggz_auto.h ./src/liboggz/oggz_auto.h
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_io.c >./src/liboggz/oggz_io.c
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_vector.c >./src/liboggz/oggz_vector.c
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/oggz_seek.c >./src/liboggz/oggz_seek.c
cp $1/src/liboggz/oggz_dlist.h ./src/liboggz/oggz_dlist.h
sed s/\#include\ \"config.h\"/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \"config.h\"\\n\#endif/g $1/src/liboggz/metric_internal.c >./src/liboggz/metric_internal.c
cp $1/AUTHORS ./AUTHORS
patch -p4 <seek.patch
patch -p4 <warning.patch
patch -p3 <oggz_off_t.patch
patch -p3 <wince.patch
