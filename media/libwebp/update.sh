#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Usage: ./update.sh <libwebp_directory>
#
# Copies the needed files from a directory containing the original
# libwebp source.

cp $1/AUTHORS .
cp $1/COPYING .
cp $1/NEWS .
cp $1/PATENTS .
cp $1/README .
cp $1/README.mux .

mkdir -p src/webp
cp $1/src/webp/*.h src/webp

mkdir -p src/dec
cp $1/src/dec/*.h src/dec
cp $1/src/dec/*.c src/dec

mkdir -p src/demux
cp $1/src/demux/demux.c src/demux

mkdir -p src/dsp
cp $1/src/dsp/*.h src/dsp
cp $1/src/dsp/*.c src/dsp
rm src/dsp/cpu.c

mkdir -p src/enc
cp $1/src/enc/*.h src/enc
cp $1/src/enc/*.c src/enc

mkdir -p src/utils
cp $1/src/utils/*.h src/utils
cp $1/src/utils/*.c src/utils
