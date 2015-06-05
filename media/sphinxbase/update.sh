#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Usage: ./update.sh <sphinxbase_directory>
#
# Copies the needed files from a directory containing the original
# sphinxbase source, and applies any local patches we're carrying.


# Create config.h
echo "#if (  defined(_WIN32) || defined(__CYGWIN__) )" > config.h
cat $1/include/win32/config.h >> config.h
echo "#else" >> config.h
cat $1/include/android/config.h >> config.h
echo "#endif" >> config.h

# Create sphinx_config.h
echo "#if (  defined(_WIN32) || defined(__CYGWIN__) )" > sphinx_config.h
cat $1/include/win32/sphinx_config.h >> sphinx_config.h
echo "#else" >> sphinx_config.h
cat $1/include/android/sphinx_config.h >> sphinx_config.h
echo "#endif" >> sphinx_config.h
sed -i '' -e 's/#define HAVE_LONG_LONG 1/\/*#define HAVE_LONG_LONG 1*\//g' sphinx_config.h

# Copy created file
cp sphinx_config.h sphinxbase/sphinx_config.h

# Copy source files
cp $1/include/sphinxbase/*.h sphinxbase/
cp $1/src/libsphinxbase/fe/*.c src/libsphinxbase/fe/
cp $1/src/libsphinxbase/fe/*.h src/libsphinxbase/fe/
cp $1/src/libsphinxbase/feat/*.c src/libsphinxbase/feat/
cp $1/src/libsphinxbase/lm/*.c src/libsphinxbase/lm/
cp $1/src/libsphinxbase/lm/*.h src/libsphinxbase/lm/
cp $1/src/libsphinxbase/util/*.c src/libsphinxbase/util/

# Apply any patches against upstream here.
patch -l -p1 < sbthread.patch
