# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Usage: ./update.sh <speexdsp_directory>
#
# Copies the needed files from a directory containing the original
# speexdsp sources.

set -e -x

cp $1/libspeexdsp/resample.c src
cp $1/libspeexdsp/resample_sse.h src/resample_sse.c
cp $1/libspeexdsp/resample_neon.h src/resample_neon.c
cp $1/libspeexdsp/arch.h src
cp $1/libspeexdsp/stack_alloc.h src
cp $1/libspeexdsp/fixed_generic.h src
cp $1/include/speex/speex_resampler.h src
cp $1/AUTHORS .
cp $1/COPYING .

# apply outstanding local patches
patch -p3 < outside-speex.patch
patch -p3 < simd-detect-runtime.patch
patch -p3 < set-skip-frac.patch
patch -p3 < hugemem.patch
patch -p3 < remove-empty-asm-clobber.patch
patch -p3 < handle-memory-error.patch
patch -p3 < fix-overflow.patch
