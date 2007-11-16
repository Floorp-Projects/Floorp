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

  for package in `find $release -maxdepth 1 -iname "*.$platform.*" | \
                  grep -v 'en-US'`
  do
    # strip the directory portion
    package=`basename $package`
    # this cannot be named $locale, because unpack_build will overwrite it
    l=`echo $package | sed -e "s/\.${platform}.*//" -e 's/.*\.//'`
    rm -rf target/*
    unpack_build $platform target $release/$package
    # check for read-only files
    find "./target" -not -perm -u=w -exec echo "FAIL read-only file" {} \;
    mkdir -p $release/diffs
    diff -r source target > $release/diffs/$platform.$l.diff
  done
done

exit 0
