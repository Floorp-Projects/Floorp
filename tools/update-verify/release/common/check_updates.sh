check_updates () {
  # called with 10 args - platform, source package, target package, update package, old updater boolean,
  # a path to the updater binary to use for the tests, a file to write diffs to, the update channel,
  # update-settings.ini values, and a flag to indicate the target is dep-signed
  update_platform=$1
  source_package=$2
  target_package=$3
  locale=$4
  use_old_updater=$5
  updater=$6
  diff_file=$7
  channel=$8
  mar_channel_IDs=$9
  update_to_dep=${10}

  # cleanup
  rm -rf source/*
  rm -rf target/*

  unpack_build $update_platform source "$source_package" $locale '' $mar_channel_IDs
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
      Darwin_ppc-gcc | Darwin_Universal-gcc3 | Darwin_x86_64-gcc3 | Darwin_x86-gcc3-u-ppc-i386 | Darwin_x86-gcc3-u-i386-x86_64 | Darwin_x86_64-gcc3-u-i386-x86_64 | Darwin_aarch64-gcc3)
          platform_dirname="*.app"
          ;;
      WINNT*)
          platform_dirname="bin"
          ;;
      Linux_x86-gcc | Linux_x86-gcc3 | Linux_x86_64-gcc3)
          platform_dirname=`echo $product | tr '[A-Z]' '[a-z]'`
          ;;
  esac

  if [ -f update/update.status ]; then rm update/update.status; fi
  if [ -f update/update.log ]; then rm update/update.log; fi

  if [ -d source/$platform_dirname ]; then
    if [ `uname | cut -c-5` == "MINGW" ]; then
      # windows
      # change /c/path/to/pwd to c:\\path\\to\\pwd
      four_backslash_pwd=$(echo $PWD | sed -e 's,^/\([a-zA-Z]\)/,\1:/,' | sed -e 's,/,\\\\,g')
      two_backslash_pwd=$(echo $PWD | sed -e 's,^/\([a-zA-Z]\)/,\1:/,' | sed -e 's,/,\\,g')
      cwd="$two_backslash_pwd\\source\\$platform_dirname"
      update_abspath="$two_backslash_pwd\\update"
    else
      # not windows
      # use ls here, because mac uses *.app, and we need to expand it
      cwd=$(ls -d $PWD/source/$platform_dirname)
      update_abspath="$PWD/update"
    fi

    cd_dir=$(ls -d ${PWD}/source/${platform_dirname})
    cd "${cd_dir}"
    set -x
    "$updater" "$update_abspath" "$cwd" "$cwd" 0
    set +x
    cd ../..
  else
    echo "TEST-UNEXPECTED-FAIL: no dir in source/$platform_dirname"
    return 1
  fi

  cat update/update.log
  update_status=`cat update/update.status`

  if [ "$update_status" != "succeeded" ]
  then
    echo "TEST-UNEXPECTED-FAIL: update status was not successful: $update_status"
    return 1
  fi

  # If we were testing an OS X mar on Linux, the unpack step copied the
  # precomplete file from Contents/Resources to the root of the install
  # to ensure the Linux updater binary could find it. However, only the
  # precomplete file in Contents/Resources was updated, which means
  # the copied version in the root of the install will usually have some
  # differences between the source and target. To prevent this false
  # positive from failing the tests, we simply remove it before diffing.
  # The precomplete file in Contents/Resources is still diffed, so we
  # don't lose any coverage by doing this.
  cd `echo "source/$platform_dirname"`
  if [[ -f "Contents/Resources/precomplete" && -f "precomplete" ]]
  then
    rm "precomplete"
  fi
  cd ../..
  cd `echo "target/$platform_dirname"`
  if [[ -f "Contents/Resources/precomplete" && -f "precomplete" ]]
  then
    rm "precomplete"
  fi
  cd ../..

  # If we are testing an OSX mar to update from a production-signed/notarized
  # build to a dep-signed one, ignore Contents/CodeResources which won't be
  # present in the target, to avoid spurious failures
  # Same applies to provisioning profiles, since we don't have them outside of prod
  if ${update_to_dep}; then
    ignore_coderesources="--ignore-missing=Contents/CodeResources --ignore-missing=Contents/embedded.provisionprofile"
  else
    ignore_coderesources=
  fi

  ../compare-directories.py source/${platform_dirname} target/${platform_dirname} ${channel} ${ignore_coderesources} > "${diff_file}"
  diffErr=$?
  cat "${diff_file}"
  if [ $diffErr == 2 ]
  then
    echo "TEST-UNEXPECTED-FAIL: differences found after update"
    return 1
  elif [ $diffErr != 0 ]
  then
    echo "TEST-UNEXPECTED-FAIL: unknown error from diff: $diffErr"
    return 3
  fi
}
