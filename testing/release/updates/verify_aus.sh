#!/bin/bash
# set -x

. ../common/unpack.sh 
. ../common/download_mars.sh

verify_aus() {
  for build_platform in $build_platforms
  do
    for locale in $locales
    do
      echo "checking $build_platform $locale"
      download_mars "https://aus2-staging.mozilla.org/update/1/$product/$release/$build_id/$build_platform/$locale/$channel/update.xml"
    done
  done
}

while read entry
do
  product="Firefox"
  channel="release"
  release=`echo $entry | awk '{print $1}'`
  build_platforms=`echo $entry | awk '{print $2}'`
  build_id=`echo $entry | awk '{print $3}'`
  locales=`cat all-locales`
  verify_aus
done < updates.cfg
