#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Usage: ./update.sh <pocketsphinx_directory>
#
# Copies the needed files from a directory containing the original
# pocketsphinx source.

cp $1/include/*.h .
cp $1/src/libpocketsphinx/*.c src/
cp $1/src/libpocketsphinx/*.h src/
