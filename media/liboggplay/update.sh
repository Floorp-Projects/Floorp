# Usage: ./update.sh <oggplay_src_directory>
#
# Copies the needed files from a directory containing the original
# liboggplay source that we need for the Mozilla HTML5 media support.
sed s/\#define\ __SSE2__\ 1//g $1/config.h >./src/liboggplay/config.h
echo "#undef HAVE_GLUT"  >>./src/liboggplay/config.h
cp $1/include/oggplay/oggplay_callback_info.h ./include/oggplay/oggplay_callback_info.h
cp $1/include/oggplay/oggplay_query.h ./include/oggplay/oggplay_query.h
cp $1/include/oggplay/oggplay_seek.h ./include/oggplay/oggplay_seek.h
cp $1/include/oggplay/oggplay_enums.h ./include/oggplay/oggplay_enums.h
cp $1/include/oggplay/oggplay_tools.h ./include/oggplay/oggplay_tools.h
cp $1/win32/config_win32.h ./include/oggplay/config_win32.h
cp $1/include/oggplay/oggplay.h ./include/oggplay/oggplay.h
cp $1/include/oggplay/oggplay_reader.h ./include/oggplay/oggplay_reader.h
cp $1/README ./README
cp $1/COPYING ./COPYING
cp $1/src/liboggplay/oggplay_buffer.c ./src/liboggplay/oggplay_buffer.c
cp $1/src/liboggplay/oggplay_tcp_reader.h ./src/liboggplay/oggplay_tcp_reader.h
cp $1/src/liboggplay/oggplay_callback_info.c ./src/liboggplay/oggplay_callback_info.c
cp $1/src/liboggplay/oggplay_tools.c ./src/liboggplay/oggplay_tools.c
cp $1/src/liboggplay/oggplay_yuv2rgb.c ./src/liboggplay/oggplay_yuv2rgb.c
cp $1/src/liboggplay/oggplay_seek.c ./src/liboggplay/oggplay_seek.c
cp $1/src/liboggplay/oggplay_buffer.h ./src/liboggplay/oggplay_buffer.h
cp $1/src/liboggplay/oggplay_file_reader.c ./src/liboggplay/oggplay_file_reader.c
cp $1/src/liboggplay/oggplay_data.h ./src/liboggplay/oggplay_data.h
cp $1/src/liboggplay/oggplay_callback.c ./src/liboggplay/oggplay_callback.c
cp $1/src/liboggplay/oggplay_file_reader.h ./src/liboggplay/oggplay_file_reader.h
cp $1/src/liboggplay/std_semaphore.h ./src/liboggplay/std_semaphore.h
cp $1/src/liboggplay/oggplay.c ./src/liboggplay/oggplay.c
cp $1/src/liboggplay/oggplay_callback.h ./src/liboggplay/oggplay_callback.h
cp $1/src/liboggplay/oggplay_tcp_reader.c ./src/liboggplay/oggplay_tcp_reader.c
cp $1/src/liboggplay/oggplay_query.c ./src/liboggplay/oggplay_query.c
sed s/\#include\ \"config_win32.h\"//g $1/src/liboggplay/oggplay_private.h >./src/liboggplay/oggplay_private.h1
sed s/\#include\ \<config.h\>/\#ifdef\ WIN32\\n\#include\ \"config_win32.h\"\\n\#else\\n\#include\ \<config.h\>\\n\#endif/g ./src/liboggplay/oggplay_private.h1 >./src/liboggplay/oggplay_private.h
rm ./src/liboggplay/oggplay_private.h1
sed s/\#ifdef\ HAVE_INTTYPES_H/\#if\ HAVE_INTTYPES_H/g $1/src/liboggplay/oggplay_data.c >./src/liboggplay/oggplay_data.c
cd ./src/liboggplay
patch <../../yuv2rgb-vanilla-fix.patch
cd ../..
