#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Usage: ./update.sh <opus_src_directory>
#
# Copies the needed files from a directory containing the original
# libopus source, and applies any local patches we're carrying.

TARGET='.'

STATIC_FILES="COPYING"
MK_FILES="opus_sources.mk celt_sources.mk silk_sources.mk \
          opus_headers.mk celt_headers.mk silk_headers.mk"

# Make sure we have a source directory
if test -z $1 || ! test -r $1/include/opus.h; then
  echo "Update the current directory from a source checkout"
  echo "usage: $0 ../opus"
  exit 1
fi

# "parse" the makefile fragments to get the list of source files
# requires GNU sed extensions
SRC_FILES=$(sed -e ':a;N;$!ba;s/#[^\n]*\(\n\)/\1/g;s/\\\n//g;s/[A-Z_]* = //g' \
             $(for file in ${MK_FILES}; do echo "$1/${file}"; done))

# pre-release versions of the code don't list opus_custom.h
# in celt_headers.mk, so we must include it manually
HDR_FILES="include/opus_custom.h"

# make sure the necessary subdirectories exist
for file in ${SRC_FILES}; do
  base=${file##*/}
  dir="${file%"${base}"}"
  if test ! -d "${TARGET}/${dir}"; then
    cmd="mkdir -p ${TARGET}/${dir}"
    echo ${cmd}
    ${cmd}
  fi
done
 
# copy files into the target directory
for file in ${STATIC_FILES} ${MK_FILES} ${SRC_FILES} ${HDR_FILES}; do
  cmd="cp $1/${file} ${TARGET}/${file}"
  echo ${cmd}
  ${cmd}
done

# query git for the revision we're copying from
if test -d $1/.git; then
  version=$(cd $1 && git describe --tags)
else
  version="UNKNOWN"
fi
echo "copied from revision ${version}"
# update README revision
sed -e "s/^The git tag\/revision used was .*/The git tag\/revision used was ${version}./" \
    ${TARGET}/README_MOZILLA > ${TARGET}/README_MOZILLA+ && \
    mv ${TARGET}/README_MOZILLA+ ${TARGET}/README_MOZILLA
# update compiled-in version string
sed -e "s/DEFINES\['OPUS_VERSION'\][ \t]*=[ \t]*'\".*\"'/DEFINES['OPUS_VERSION'] = '\"${version}-mozilla\"'/" \
    ${TARGET}/moz.build > ${TARGET}/moz.build+ && \
    mv ${TARGET}/moz.build+ ${TARGET}/moz.build

# apply outstanding local patches
# ... no patches to apply ...
