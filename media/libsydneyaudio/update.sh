# Usage: ./update.sh <libsydneyaudio_src_directory>
#
# Copies the needed files from a directory containing the original
# libsydneyaudio source that we need for the Mozilla HTML5 media support.
cp $1/include/sydney_audio.h include/sydney_audio.h
cp $1/src/*.c src/
cp $1/AUTHORS ./AUTHORS
patch -p4 <pause-resume.patch
patch -p4 <include-CoreServices.patch
patch -p4 <sydney_os2_base.patch
patch -p4 <sydney_os2_moz.patch
patch -p3 <bug495794_closeAudio.patch
patch -p3 <bug495558_alsa_endian.patch
patch -p3 <bug525401_drain_deadlock.patch
patch -p3 <sydney_aix.patch
patch -p3 <bug562488_oss_destroy_crash.patch
patch -p3 <bug564734-win32-drain.patch
patch -p3 <sydney_android.patch
