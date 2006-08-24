#!/bin/bash
#set -x

. ../common/unpack.sh 
. ../common/download_mars.sh
. ../common/download_builds.sh
. ../common/check_updates.sh

product="Firefox"
channel="release"
latest="1.5.0.6"

while read entry
do
  release=`echo $entry | cut -d' ' -f 1`
  platforms=`echo $entry | cut -d' ' -f 2`
  build_id=`echo $entry | cut -d' ' -f 3`
  locales=`echo $entry | cut -d' ' -f 4-`
  for platform in $platforms
  do
    for locale in $locales
    do
      for patch_type in "partial complete"
      do
        download_mars "https://aus2.mozilla.org/update/1/$product/$release/$build_id/$platform/$locale/$channel/update.xml" $patch_type
        err=$?
        if [ "$err" != "0" ]; then
          echo "FAIL: download_mars returned non-zero exit code: $err" |tee /dev/stderr
          continue
        fi
        download_builds
        err=$?
        if [ "$err" != "0" ]; then
          echo "FAIL: download_builds returned non-zero exit code: $err" |tee /dev/stderr
          continue
        fi
        check_updates "$source_platform" "downloads/$source_file" "downloads/$target_file"
        err=$?
        if [ "$err" != "0" ]; then
          echo "FAIL: check_update returned non-zero exit code for $source_platform downloads/$source_file vs. downloads/$target_file: $err" |tee /dev/stderr
          continue
        fi
      done
    done
  done
done < updates.cfg

