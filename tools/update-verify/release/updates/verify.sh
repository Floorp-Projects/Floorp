#!/bin/bash
#set -x

. ../common/cached_download.sh
. ../common/unpack.sh
. ../common/download_mars.sh
. ../common/download_builds.sh
. ../common/check_updates.sh

clear_cache
create_cache

ftp_server_to="http://stage.mozilla.org/pub/mozilla.org"
ftp_server_from="http://stage.mozilla.org/pub/mozilla.org"
aus_server="https://aus4.mozilla.org"
to=""
to_build_id=""
to_app_version=""
to_display_version=""
override_certs=""
diff_summary_log="$PWD/diff-summary.log"
if [ -e ${diff_summary_log} ]; then
  rm ${diff_summary_log}
fi
touch ${diff_summary_log}

pushd `dirname $0` &>/dev/null
MY_DIR=$(pwd)
popd &>/dev/null
retry="$MY_DIR/../../../../mach python -m redo.cmd -s 1 -a 3"
cert_replacer="$MY_DIR/../replace-updater-certs.py"

dep_overrides="nightly_aurora_level3_primary.der dep1.der nightly_aurora_level3_secondary.der dep2.der release_primary.der dep1.der release_secondary.der dep2.der sha1/release_primary.der sha1/dep1.der sha1/release_secondary.der sha1/dep2.der"
nightly_overrides="dep1.der nightly_aurora_level3_primary.der dep2.der nightly_aurora_level3_secondary.der release_primary.der nightly_aurora_level3_primary.der release_secondary.der nightly_aurora_level3_secondary.der"
release_overrides="dep1.der release_primary.der dep2.der release_secondary.der nightly_aurora_level3_primary.der release_primary.der nightly_aurora_level3_secondary.der release_secondary.der"

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
  patch_types="complete"
  use_old_updater=0
  mar_channel_IDs=""
  updater_package=""
  eval $entry

  # the arguments for updater changed in Gecko 34/SeaMonkey 2.31
  major_version=`echo $release | cut -f1 -d.`
  if [[ "$product" == "seamonkey" ]]; then
    minor_version=`echo $release | cut -f2 -d.`
    if [[ $major_version -le 2 && $minor_version -lt 31 ]]; then
      use_old_updater=1
    fi
  elif [[ $major_version -lt 34 ]]; then
      use_old_updater=1
  fi

  # Note: cross platform tests seem to work for everything except Mac-on-Windows.
  # We probably don't care about this use case.
  if [[ "$updater_package" == "" ]]; then
    updater_package="$from"
  fi

  for locale in $locales
  do
    rm -f update/partial.size update/complete.size
    for patch_type in $patch_types
    do
      update_path="${product}/${release}/${build_id}/${platform}/${locale}/${channel}/default/default/default"
      if [ "$runmode" == "$MARS_ONLY" ] || [ "$runmode" == "$COMPLETE" ] ||
         [ "$runmode" == "$TEST_ONLY" ]
      then
        if [ "$runmode" == "$TEST_ONLY" ]
        then
          download_mars "${aus_server}/update/3/${update_path}/default/update.xml?force=1" ${patch_type} 1 \
            "${to_build_id}" "${to_app_version}" "${to_display_version}"
          err=$?
        else
          download_mars "${aus_server}/update/3/${update_path}/update.xml?force=1" ${patch_type} 0 \
            "${to_build_id}" "${to_app_version}" "${to_display_version}"
          err=$?
        fi
        if [ "$err" != "0" ]; then
          echo "TEST-UNEXPECTED-FAIL: [${release} ${locale} ${patch_type}] download_mars returned non-zero exit code: ${err}"
          continue
        fi
      else
        mkdir -p updates/${update_path}/complete
        mkdir -p updates/${update_path}/partial
        $retry wget -q -O ${patch_type} updates/${update_path}/${patch_type}/update.xml "${aus_server}/update/3/${update_path}/update.xml?force=1"

      fi
      if [ "$runmode" == "$COMPLETE" ]
      then
        if [ -z "$from" ] || [ -z "$to" ]
        then
          continue
        fi

        updater_platform=""
        updater_package_url=`echo "${ftp_server_from}${updater_package}" | sed "s/%locale%/${locale}/"`
        updater_package_filename=`basename "$updater_package_url"`
        case $updater_package_filename in
          *dmg)
            platform_dirname="*.app"
            updater_bins="Contents/MacOS/updater.app/Contents/MacOS/updater Contents/MacOS/updater.app/Contents/MacOS/org.mozilla.updater"
            updater_platform="mac"
            ;;
          *exe)
            updater_package_url=`echo "${updater_package_url}" | sed "s/ja-JP-mac/ja/"`
            platform_dirname="bin"
            updater_bins="updater.exe"
            updater_platform="win32"
            ;;
          *bz2)
            updater_package_url=`echo "${updater_package_url}" | sed "s/ja-JP-mac/ja/"`
            platform_dirname=`echo $product | tr '[A-Z]' '[a-z]'`
            updater_bins="updater"
            updater_platform="linux"
            ;;
          *)
            echo "Couldn't detect updater platform"
            exit 1
            ;;
        esac

        rm -rf updater/*
        cached_download "${updater_package_filename}" "${updater_package_url}"
        unpack_build "$updater_platform" updater "$updater_package_filename" "$locale"

        # Even on Windows, we want Unix-style paths for the updater, because of MSYS.
        cwd=$(\ls -d $PWD/updater/$platform_dirname)
        # Bug 1209376. Linux updater linked against other libraries in the installation directory
        export LD_LIBRARY_PATH=$cwd
        updater="null"
        for updater_bin in $updater_bins; do
            if [ -e "$cwd/$updater_bin" ]; then
                echo "Found updater at $updater_bin"
                updater="$cwd/$updater_bin"
                break
            fi
        done

        if [ ! -z "$override_certs" ]; then
            echo "Replacing certs in updater binary"
            cp "${updater}" "${updater}.orig"
            case ${override_certs} in
              dep)
                overrides=${dep_overrides}
                ;;
              nightly)
                overrides=${nightly_overrides}
                ;;
              release)
                overrides=${release_overrides}
                ;;
              *)
                echo "Unknown override cert - skipping"
                ;;
            esac
            python "${cert_replacer}" "${MY_DIR}/../mar_certs" "${updater}.orig" "${updater}" ${overrides}
        else
            echo "override_certs is '${override_certs}', not replacing any certificates"
        fi

        if [ "$updater" == "null" ]; then
            echo "Couldn't find updater binary"
            continue
        fi

        from_path=`echo $from | sed "s/%locale%/${locale}/"`
        to_path=`echo $to | sed "s/%locale%/${locale}/"`
        download_builds "${ftp_server_from}${from_path}" "${ftp_server_to}${to_path}"
        err=$?
        if [ "$err" != "0" ]; then
          echo "TEST-UNEXPECTED-FAIL: [$release $locale $patch_type] download_builds returned non-zero exit code: $err"
          continue
        fi
        source_file=`basename "$from_path"`
        target_file=`basename "$to_path"`
        diff_file="results.diff"
        if [ -e ${diff_file} ]; then
          rm ${diff_file}
        fi
        check_updates "${platform}" "downloads/${source_file}" "downloads/${target_file}" ${locale} ${use_old_updater} ${updater} ${diff_file} ${channel} ${mar_channel_IDs}
        err=$?
        if [ "$err" == "0" ]; then
          continue
        elif [ "$err" == "1" ]; then
          echo "TEST-UNEXPECTED-FAIL: [$release $locale $patch_type] check_updates returned failure for $platform downloads/$source_file vs. downloads/$target_file: $err"
        elif [ "$err" == "2" ]; then
          echo "WARN: [$release $locale $patch_type] check_updates returned warning for $platform downloads/$source_file vs. downloads/$target_file: $err"
        else
          echo "TEST-UNEXPECTED-FAIL: [$release $locale $patch_type] check_updates returned unknown error for $platform downloads/$source_file vs. downloads/$target_file: $err"
        fi

        if [ -s ${diff_file} ]; then
          echo "Found diffs for ${patch_type} update from ${aus_server}/update/3/${update_path}/update.xml?force=1" >> ${diff_summary_log}
          cat ${diff_file} >> ${diff_summary_log}
          echo "" >> ${diff_summary_log}
        fi
      fi
    done
    if [ -f update/partial.size ] && [ -f update/complete.size ]; then
        partial_size=`cat update/partial.size`
        complete_size=`cat update/complete.size`
        if [ $partial_size -gt $complete_size ]; then
            echo "TEST-UNEXPECTED-FAIL: [$release $locale $patch_type] partial updates are larger than complete updates"
        elif [ $partial_size -eq $complete_size ]; then
            echo "WARN: [$release $locale $patch_type] partial updates are the same size as complete updates, this should only happen for major updates"
        else
            echo "SUCCESS: [$release $locale $patch_type] partial updates are smaller than complete updates, all is well in the universe"
        fi
    fi
  done
done < $config_file

clear_cache
