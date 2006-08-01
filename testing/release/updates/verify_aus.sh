#!/bin/bash
# set -x

. ../common/unpack.sh 
. ../common/download_mars.sh

build_id="2006050817"
product="Firefox"
release="1.5.0.4"
channel="release"


for build_platform in Darwin_Universal-gcc3 # WINNT_x86-msvc Darwin_Universal-gcc3 Linux_x86-gcc3
do

  for locale in `cat all-locales`
  #for locale in ko ja ja-JP-mac zh-CN zh-TW
  #for locale in en-US
  do

    echo "checking $build_platform $locale"

    download_mars "https://aus2.mozilla.org/update/1/$product/$release/$build_id/$build_platform/$locale/$channel/update.xml"

  done
  
done
