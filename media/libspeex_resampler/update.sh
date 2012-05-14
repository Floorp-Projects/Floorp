# Usage: ./update.sh <libspeex_src_directory>
#
# Copies the needed files from a directory containing the original
# libspeex sources that we need for HTML5 media playback rate change.
cp $1/libspeex/resample.c src
cp $1/libspeex/arch.h src
cp $1/libspeex/stack_alloc.h src
cp $1/libspeex/fixed_generic.h src
cp $1/include/speex/speex_resampler.h src
cp $1/AUTHORS .
cp $1/COPYING .
