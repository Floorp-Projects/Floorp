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
  # check for read-only files
  find "./source" -not -perm -u=w -exec echo "FAIL read-only file" {} \;
  locales=`ls $release/*.${platform}.* | grep -v en-US | cut -d\. -f8`
  for l in $locales
  do
    rm -rf target/*
    unpack_build ${platform} target $release/*.${l}.${platform}.*
    # check for read-only files
    find "./target" -not -perm -u=w -exec echo "FAIL read-only file" {} \;
    mkdir -p $release/diffs
    diff -r source target > $release/diffs/${platform}.${l}.diff
  done
done

exit 0
