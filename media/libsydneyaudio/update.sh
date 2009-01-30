# Usage: ./update.sh <oggplay_src_directory>
#
# Copies the needed files from a directory containing the original
# liboggplay source that we need for the Mozilla HTML5 media support.
cp $1/include/sydney_audio.h include/sydney_audio.h
cp $1/src/*.c src/
cp $1/AUTHORS ./AUTHORS
patch -p3 < 469698_mac_stream_endian.patch
