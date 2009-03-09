# Usage: cp $1/update.sh <libfishsound_src_directory>
#
# Copies the needed files from a directory containing the original
# libfishsound source that we need for the Mozilla HTML5 media support.
cp $1/config.h ./include/fishsound/config.h
echo "#undef FS_ENCODE" >>./include/fishsound/config.h
echo "#define FS_ENCODE 0" >>./include/fishsound/config.h
echo "#undef HAVE_FLAC" >>./include/fishsound/config.h
echo "#define HAVE_FLAC 0" >>./include/fishsound/config.h
echo "#undef HAVE_OGGZ" >>./include/fishsound/config.h
echo "#define HAVE_OGGZ 1" >>./include/fishsound/config.h
echo "#undef HAVE_SPEEX" >>./include/fishsound/config.h
echo "#define HAVE_SPEEX 0" >>./include/fishsound/config.h
echo "#undef HAVE_VORBIS" >>./include/fishsound/config.h
echo "#define HAVE_VORBIS 1" >>./include/fishsound/config.h
echo "#undef HAVE_VORBISENC" >>./include/fishsound/config.h
echo "#define HAVE_VORBISENC 0" >>./include/fishsound/config.h
echo "#undef DEBUG" >>./include/fishsound/config.h
cp $1/include/fishsound/encode.h ./include/fishsound/encode.h
cp $1/include/fishsound/comments.h ./include/fishsound/comments.h
cp $1/include/fishsound/deprecated.h ./include/fishsound/deprecated.h
cp $1/include/fishsound/fishsound.h ./include/fishsound/fishsound.h
cp $1/include/fishsound/constants.h ./include/fishsound/constants.h
cp $1/include/fishsound/decode.h ./include/fishsound/decode.h
cp $1/COPYING ./COPYING
cp $1/README ./README
cp ./include/fishsound/config.h ./src/libfishsound/config.h
cp $1/src/libfishsound/decode.c ./src/libfishsound/fishsound_decode.c
cp $1/src/libfishsound/fishsound.c ./src/libfishsound/fishsound.c
sed s/\#include\ \<vorbis\\/vorbisenc.h\>/\#if\ HAVE_VORBISENC\\n\#include\ \<vorbis\\/vorbisenc.h\>\\n\#endif/g  $1/src/libfishsound/vorbis.c >./src/libfishsound/fishsound_vorbis.c
cp $1/src/libfishsound/flac.c ./src/libfishsound/fishsound_flac.c
cp $1/src/libfishsound/comments.c ./src/libfishsound/fishsound_comments.c
cp $1/src/libfishsound/private.h ./src/libfishsound/private.h
cp $1/src/libfishsound/fs_compat.h ./src/libfishsound/fs_compat.h
cp $1/src/libfishsound/speex.c ./src/libfishsound/fishsound_speex.c
cp $1/src/libfishsound/encode.c ./src/libfishsound/fishsound_encode.c
cp $1/src/libfishsound/fs_vector.h ./src/libfishsound/fs_vector.h
cp $1/src/libfishsound/fs_vector.c ./src/libfishsound/fs_vector.c
cp $1/src/libfishsound/convert.h ./src/libfishsound/convert.h
cp $1/AUTHORS ./AUTHORS
patch -p4 <endian.patch
