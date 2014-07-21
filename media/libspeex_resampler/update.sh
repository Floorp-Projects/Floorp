# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Usage: ./update.sh <opus-tools_directory>
#
# Copies the needed files from a directory containing the original
# opus-tools sources.

set -e -x

cp $1/src/resample.c src
cp $1/src/resample_sse.h src
cp $1/src/arch.h src
cp $1/src/stack_alloc.h src
cp $1/src/speex_resampler.h src
cp $1/AUTHORS .
cp $1/COPYING .

# apply outstanding local patches
patch -p3 < outside-speex.patch
patch -p1 < sse-detect-runtime.patch
patch -p3 < reset.patch
patch -p3 < set-skip-frac.patch
