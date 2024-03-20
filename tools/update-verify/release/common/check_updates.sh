#!/bin/bash

check_updates () {
  # called with 10 args - platform, source package, target package, update package, old updater boolean,
  # a path to the updater binary to use for the tests, a file to write diffs to, the update channel,
  # update-settings.ini values, and a flag to indicate the target is dep-signed
  update_platform=$1
  source_package=$2
  target_package=$3
  locale=$4
  updater=$5
  diff_file=$6
  channel=$7
  mar_channel_IDs=$8
  update_to_dep=$9
  local mac_update_settings_dir_override
  mac_update_settings_dir_override=${10}
  local product
  product=${11}

  # cleanup
  rm -rf source/*
  rm -rf target/*

  # $mac_update_settings_dir_override allows unpack_build to find a host platform appropriate
  # `update-settings.ini` file, which is needed to successfully run the updater later in this
  # function.
  if ! unpack_build "$update_platform" source "$source_package" "$locale" '' "$mar_channel_IDs" "$mac_update_settings_dir_override" "$product"; then
    echo "FAILED: cannot unpack_build $update_platform source $source_package"
    return 1
  fi
  # Unlike unpacking the `source` build, we don't actually _need_ $mac_update_settings_dir_override
  # here to succesfully apply the update, but its usage in `source` causes an `update-settings.ini`
  # file to be present in the directory we diff, which means we either also need it present in the
  # `target` directory, or to remove it after the update is applied. The latter was chosen
  # because it keeps the workaround close together (as opposed to just above this, and then much
  # further down).
  if ! unpack_build "$update_platform" target "$target_package" "$locale" '' '' "$mac_update_settings_dir_override" "$product"; then
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
          platform_dirname=$(echo "$product" | tr '[:upper:]' '[:lower:]')
          ;;
  esac

  if [ -f update/update.status ]; then rm update/update.status; fi
  if [ -f update/update.log ]; then rm update/update.log; fi

  # This check is disabled because we rely on glob expansion here
  # shellcheck disable=SC2086
  if [ -d source/$platform_dirname ]; then
    if [ "$(uname | cut -c-5)" == "MINGW" ]; then
      # windows
      # change /c/path/to/pwd to c:\\path\\to\\pwd
      two_backslash_pwd=$(echo "$PWD" | sed -e 's,^/\([a-zA-Z]\)/,\1:/,' | sed -e 's,/,\\,g')
      cwd="$two_backslash_pwd\\source\\$platform_dirname"
      update_abspath="$two_backslash_pwd\\update"
    else
      # not windows
      # use ls here, because mac uses *.app, and we need to expand it
      # This check is disabled because we rely on glob expansion here
      # shellcheck disable=SC2086
      cwd=$(ls -d $PWD/source/$platform_dirname)
      update_abspath="$PWD/update"
    fi

    # This check is disabled because we rely on glob expansion here
    # shellcheck disable=SC2086
    cd_dir=$(ls -d ${PWD}/source/${platform_dirname})
    cd "${cd_dir}" || (echo "TEST-UNEXPECTED-FAIL: couldn't cd to ${cd_dir}" && return 1)
    set -x
    "$updater" "$update_abspath" "$cwd" "$cwd" 0
    set +x
    cd ../..
  else
    echo "TEST-UNEXPECTED-FAIL: no dir in source/$platform_dirname"
    return 1
  fi

  cat update/update.log
  update_status=$(cat update/update.status)

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
  # This check is disabled because we rely on glob expansion here
  # shellcheck disable=SC2086
  cd source/$platform_dirname || (echo "TEST-UNEXPECTED-FAIL: couldn't cd to source/${platform_dirname}" && exit 1)
  if [[ -f "Contents/Resources/precomplete" && -f "precomplete" ]]
  then
    rm "precomplete"
  fi
  cd ../..
  # This check is disabled because we rely on glob expansion here
  # shellcheck disable=SC2086
  cd target/$platform_dirname || (echo "TEST-UNEXPECTED-FAIL: couldn't cd to target/${platform_dirname}" && exit 1)
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

  # On Mac, there are two Frameworks that are not included with updates, and
  # which change with every build. Because of this, we ignore differences in
  # them in `compare-directories.py`. The best verification we can do for them
  # is that they still exist.
  if [[ $update_platform == Darwin_* ]]; then
    # This check is disabled because we rely on glob expansion here
    # shellcheck disable=SC2086
    if ! compgen -G source/${platform_dirname}/Contents/MacOS/updater.app/Contents/Frameworks/UpdateSettings.framework >/dev/null; then
      echo "TEST-UNEXPECTED-FAIL: UpdateSettings.framework doesn't exist after update"
      return 4
    fi
    # This check is disabled because we rely on glob expansion here
    # shellcheck disable=SC2086
    if ! compgen -G source/${platform_dirname}/Contents/Frameworks/ChannelPrefs.framework >/dev/null; then
      echo "TEST-UNEXPECTED-FAIL: ChannelPrefs.framework doesn't exist after update"
      return 5
    fi
  fi

  # This check is disabled because we rely on glob expansion here
  # shellcheck disable=SC2086
  ../compare-directories.py source/${platform_dirname} target/${platform_dirname} "${channel}" ${ignore_coderesources} > "${diff_file}"
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
