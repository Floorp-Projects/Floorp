#!/bin/bash
#set -x

. ../common/unpack.sh 
. ../common/download_mars.sh
. ../common/download_builds.sh
. ../common/check_updates.sh

ftp_server="http://stage.mozilla.org/pub/mozilla.org"
aus_server="https://aus2-staging.mozilla.org"

runmode=0
config_file="updates.cfg"
UPDATE_ONLY=1
TEST_ONLY=2
MARS_ONLY=3
COMPLETE=4

usage()
{
  echo "Usage: verify.sh [OPTION] [CONFIG_FILE]"
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

if [ -n "$arg" ]
then
  config_file=$arg
  echo "Using config file $config_file"
else
  echo "Using default config file $config_file"
fi

if [ "$runmode" == "0" ]
then
  usage
  exit 0
fi

while read entry
do
  # initialize all config variables
  release="" 
  product="" 
  platform="" 
  build_id="" 
  locales=""
  channel=""
  from=""
  to=""
  eval $entry
  for locale in $locales
  do
    for patch_type in partial complete
    do
      if [ "$runmode" == "$MARS_ONLY" ] || [ "$runmode" == "$COMPLETE" ] ||
         [ "$runmode" == "$TEST_ONLY" ]
      then
        if [ "$runmode" == "$TEST_ONLY" ]
        then
          download_mars "${aus_server}/update/1/$product/$release/$build_id/$platform/$locale/$channel/update.xml" $patch_type 1
          err=$?
        else
          download_mars "${aus_server}/update/1/$product/$release/$build_id/$platform/$locale/$channel/update.xml" $patch_type
          err=$?
        fi
        if [ "$err" != "0" ]; then
          echo "FAIL: download_mars returned non-zero exit code: $err"
          continue
        fi
      else
        update_path="$product/$release/$build_id/$platform/$locale/$channel"
        mkdir -p updates/$update_path/complete
        mkdir -p updates/$update_path/partial
        curl -ks "${aus_server}/update/1/$update_path/update.xml" $patch_type > updates/$update_path/$patch_type/update.xml

      fi
      if [ "$runmode" == "$COMPLETE" ]
      then
        if [ -z "$from" ] || [ -z "$to" ]
        then
          continue
        fi
        from_path=`echo $from | sed "s/%locale%/${locale}/"`
        to_path=`echo $to | sed "s/%locale%/${locale}/"`
        download_builds "${ftp_server}/${from_path}" "${ftp_server}/${to_path}"
        err=$?
        if [ "$err" != "0" ]; then
          echo "FAIL: download_builds returned non-zero exit code: $err"
          continue
        fi
        source_file=`basename "$from_path"`
        target_file=`basename "$to_path"`
        check_updates "$platform" "downloads/$source_file" "downloads/$target_file"
        err=$?
        if [ "$err" != "0" ]; then
          echo "WARN: check_update returned non-zero exit code for $platform downloads/$source_file vs. downloads/$target_file: $err"
          continue
        fi
      fi
    done
  done
done < $config_file

