# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Usage: ./update.sh <SoundTouch_src_directory>
#
# Copies the needed files from a directory containing the original
# soundtouch sources that we need for HTML5 media playback rate change.

cp $1/COPYING.TXT .
cp $1/source/SoundTouch/AAFilter.cpp src
cp $1/source/SoundTouch/AAFilter.h src
cp $1/source/SoundTouch/cpu_detect.h src
cp $1/source/SoundTouch/cpu_detect_x86.cpp src
cp $1/source/SoundTouch/FIFOSampleBuffer.cpp src
cp $1/source/SoundTouch/FIRFilter.cpp src
cp $1/source/SoundTouch/FIRFilter.h src
cp $1/source/SoundTouch/mmx_optimized.cpp src
cp $1/source/SoundTouch/RateTransposer.cpp src
cp $1/source/SoundTouch/RateTransposer.h src
cp $1/source/SoundTouch/SoundTouch.cpp src
cp $1/source/SoundTouch/sse_optimized.cpp src
cp $1/source/SoundTouch/TDStretch.cpp src
cp $1/source/SoundTouch/TDStretch.h src
cp $1/include/SoundTouch.h src
cp $1/include/FIFOSampleBuffer.h src
cp $1/include/FIFOSamplePipe.h src
cp $1/include/SoundTouch.h src
cp $1/include/STTypes.h src

# Remove the Windows line ending characters from the files.
for i in src/*
do
  cat $i | tr -d '\015' > $i.lf
  mv $i.lf $i
done

# Patch the imported files.
patch -p1 < moz-libsoundtouch.patch

