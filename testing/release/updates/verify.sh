#!/bin/bash
#set -x

. ../common/unpack.sh 
. ../common/download_mars.sh
. ../common/download_builds.sh
. ../common/check_updates.sh

product="Firefox"
channel="release"
latest="1.5.0.7"

runmode=0
UPDATE_ONLY=1
TEST_ONLY=2
MARS_ONLY=3
COMPLETE=4

usage()
{
  echo "Usage: verify.sh [OPTION]"
  echo "    -u, --update-only      only download update.xml"
  echo "    -t, --test-only        only test that MARs exist"
  echo "    -m, --mars-only        only test MARs"
  echo "    -c, --complete         complete upgrade test"
}

if [ -z "$*" ]
then
  usage
  exit 0
fi

pass_arg_count=0
while [ "$#" -gt "$pass_arg_count" ]
do
  case "$1" in
    -u | --update-only)
      runmode=$UPDATE_ONLY
      shift
      ;;
    -t | --test-only)
      runmode=$TEST_ONLY
      shift
      ;;
    -m | --mars-only)
      runmode=$MARS_ONLY
      shift
      ;;
    -c | --complete)
      runmode=$COMPLETE
      shift
      ;;
    *)
      # Move the unrecognized arg to the end of the list
      arg="$1"
      shift
      set -- "$@" "$arg"
      pass_arg_count=`expr $pass_arg_count + 1`
  esac
done

if [ "$runmode" == "0" ]
then
  usage
  exit 0
fi

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
      for patch_type in partial complete
      do
        if [ "$runmode" == "$MARS_ONLY" ] || [ "$runmode" == "$COMPLETE" ] ||
           [ "$runmode" == "$TEST_ONLY" ]
        then
          if [ "$runmode" == "$TEST_ONLY" ]
          then
            download_mars "https://aus2.mozilla.org/update/1/$product/$release/$build_id/$platform/$locale/$channel/update.xml" $patch_type 1
            err=$?
          else
            download_mars "https://aus2.mozilla.org/update/1/$product/$release/$build_id/$platform/$locale/$channel/update.xml" $patch_type
            err=$?
          fi
          if [ "$err" != "0" ]; then
            echo "FAIL: download_mars returned non-zero exit code: $err" |tee /dev/stderr
            continue
          fi
        else
          update_path="$product/$release/$build_id/$platform/$locale/$channel"
          mkdir -p updates/$update_path/complete
          mkdir -p updates/$update_path/partial
          curl -ks "https://aus2.mozilla.org/update/1/$update_path/update.xml" $patch_type > updates/$update_path/$patch_type/update.xml

        fi
        if [ "$runmode" == "$COMPLETE" ]
        then
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
        fi
      done
    done
  done
done < updates.cfg

