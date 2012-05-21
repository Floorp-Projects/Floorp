# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

check_updates () {
  # called with 4 args - platform, source package, target package, update package
  update_platform=$1
  source_package=$2
  target_package=$3
  locale=$4

  # cleanup
  rm -rf source/*
  rm -rf target/*

  unpack_build $update_platform source "$source_package" $locale
  if [ "$?" != "0" ]; then
    echo "FAILED: cannot unpack_build $update_platform source $source_package"
    return 1
  fi
  unpack_build $update_platform target "$target_package" $locale 
  if [ "$?" != "0" ]; then
    echo "FAILED: cannot unpack_build $update_platform target $target_package"
    return 1
  fi
  
  case $update_platform in
      Darwin_ppc-gcc | Darwin_Universal-gcc3) 
          platform_dirname="*.app"
          updater="Contents/MacOS/updater.app/Contents/MacOS/updater"
          ;;
      WINNT_x86-msvc) 
          platform_dirname="bin"
          updater="updater.exe"
          ;;
      Linux_x86-gcc | Linux_x86-gcc3) 
          platform_dirname=`echo $product | tr '[A-Z]' '[a-z]'`
          updater="updater"
          ;;
  esac

  if [ -f update/update.status ]; then rm update/update.status; fi
  if [ -f update/update.log ]; then rm update/update.log; fi

  if [ -d source/$platform_dirname ]; then
    cd source/$platform_dirname;
    cp $updater ../../update
    ../../update/updater ../../update 0
    cd ../..
  else
    echo "FAIL: no dir in source/$platform_dirname"
    return 1
  fi

  cat update/update.log
  update_status=`cat update/update.status`

  if [ "$update_status" != "succeeded" ]
  then
    echo "FAIL: update status was not succeeded: $update_status"
    return 1
  fi

  diff -r source/$platform_dirname target/$platform_dirname  > results.diff
  diffErr=$?
  cat results.diff
  grep '^Binary files' results.diff > /dev/null
  grepErr=$?
  if [ $grepErr == 0 ]
  then
    echo "FAIL: binary files found in diff"
    return 1
  elif [ $grepErr == 1 ]
  then
    if [ -s results.diff ]
    then
      echo "WARN: non-binary files found in diff"
      return 2
    fi
  else
    echo "FAIL: unknown error from grep: $grepErr"
    return 3
  fi
  if [ $diffErr != 0 ]
  then
    echo "FAIL: unknown error from diff: $diffErr"
    return 3
  fi
}
