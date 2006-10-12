#!/bin/bash

. ../common/unpack.sh

release=$1

if [ -z "$release" ]
then
  echo "Syntax: $0 <release_dir>"
  exit 1
fi

for platform in linux-i686 win32 mac
do
  rm -rf source/*
  # unpack_build platform dir_name pkg_file
  unpack_build $platform source $release/*.en-US.${platform}.*
  locales=`ls $release/*.${platform}.* | grep -v en-US | cut -d\. -f5`
  for locale in $locales
  do
    rm -rf target/*
    unpack_build ${platform} target $release/*.${locale}.${platform}.*
    mkdir -p $release/diffs
    diff -r source target > $release/diffs/${platform}.${locale}.diff
    diff -r source target > ${platform}.${locale}.diff
  done
done
