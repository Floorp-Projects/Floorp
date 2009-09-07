# Usage: ./update.sh <oggplay_src_directory>
#
# Copies the needed files from a directory containing the original
# liboggplay source that we need for the Mozilla HTML5 media support.
cp $1/include/sydney_audio.h include/sydney_audio.h
cp $1/src/*.c src/
cp $1/AUTHORS ./AUTHORS
patch -p4 <pause-resume.patch
patch -p4 <include-CoreServices.patch
patch -p4 <sydney_os2_base.patch
patch -p4 <sydney_os2_moz.patch
patch -p3 <bug495794_closeAudio.patch
patch -p3 < bug495558_alsa_endian.patch
