#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Code shared by update packaging scripts.
# Author: Darin Fisher
#

# -----------------------------------------------------------------------------
# By default just assume that these tools exist on our path
MAR=${MAR:-mar}
MBSDIFF=${MBSDIFF:-mbsdiff}
if [[ -z "${MAR_OLD_FORMAT}" ]]; then
  while read -r XZ
  do
    if ${XZ} --version > /dev/null 2>&1
    then
      break
    fi
  done << EOM
${XZ:-xz}
$(dirname "$(dirname "$(dirname "$0")")")/xz/xz.exe
$(dirname "$(dirname "$(dirname "$(dirname "$0")")")")/xz/xz.exe
EOM

  if [ -z "${XZ}" ]
  then
      echo "xz was not found on this system!"
      echo "exiting"
      exit 1
  fi
else
  MAR_OLD_FORMAT=1
  BZIP2=${BZIP2:-bzip2}
fi

# -----------------------------------------------------------------------------
# Helper routines

notice() {
  echo "$*" 1>&2
}

get_file_size() {
  stat -c"%s" "$1"
}

copy_perm() {
  reference="$1"
  target="$2"

  if [ -x "$reference" ]; then
    chmod 0755 "$target"
  else
    chmod 0644 "$target"
  fi
}

make_add_instruction() {
  f="$1"
  filev2="$2"
  # The third param will be an empty string when a file add instruction is only
  # needed in the version 2 manifest. This only happens when the file has an
  # add-if-not instruction in the version 3 manifest. This is due to the
  # precomplete file prior to the version 3 manifest having a remove instruction
  # for this file so the file is removed before applying a complete update.
  filev3="$3"

  # Used to log to the console
  if [ "$4" ]; then
    forced=" (forced)"
  else
    forced=
  fi

  if [[ "$f" =~ distribution/extensions/.*/ ]]
  then
    # Use the subdirectory of the extensions folder as the file to test
    # before performing this add instruction.
    testdir=$(echo "$f" | sed 's/\(.*distribution\/extensions\/[^\/]*\)\/.*/\1/')
    notice "     add-if \"$testdir\" \"$f\""
    printf 'add-if "%s" "%s"\n' "${testdir}" "${f}" >> "${filev2}"
    if [ -n "${filev3}" ]; then
      printf 'add-if "%s" "%s"\n' "${testdir}" "${f}" >> "${filev3}"
    fi
  else
    notice "        add \"$f\"$forced"
    printf 'add "%s"\n' "${f}" >> "${filev2}"
    if [ ! "$filev3" = "" ]; then
      printf 'add "%s"\n' "${f}" >> "${filev3}"
    fi
  fi
}

check_for_add_if_not_update() {
  add_if_not_file_chk="$1"

  if [ "$(basename "$add_if_not_file_chk")" = "channel-prefs.js" ] || \
     [ "$(basename "$add_if_not_file_chk")" = "update-settings.ini" ]
  then
    return 0;
  fi
  return 1;
}

check_for_add_to_manifestv2() {
  add_if_not_file_chk="$1"

  if [ "$(basename "$add_if_not_file_chk")" = "update-settings.ini" ]
  then
    return 0;
  fi
  return 1;
}

make_add_if_not_instruction() {
  f="$1"
  filev3="$2"

  notice " add-if-not \"$f\" \"$f\""
  printf 'add-if-not "%s" "%s"\n' "${f}" "${f}" >> "${filev3}"
}

make_patch_instruction() {
  f="$1"
  filev2="$2"
  filev3="$3"

  if [[ "$f" =~ distribution/extensions/.*/ ]]
  then
    # Use the subdirectory of the extensions folder as the file to test
    # before performing this add instruction.
    testdir=$(echo "$f" | sed 's/\(.*distribution\/extensions\/[^\/]*\)\/.*/\1/')
    notice "   patch-if \"$testdir\" \"$f.patch\" \"$f\""
    printf 'patch-if "%s" "%s.patch" "%s"\n' "${testdir}" "${f}" "${f}" >> "${filev2}"
    printf 'patch-if "%s" "%s.patch" "%s"\n' "${testdir}" "${f}" "${f}" >> "${filev3}"
  else
    notice "      patch \"$f.patch\" \"$f\""
    printf 'patch "%s.patch" "%s"\n' "${f}" "${f}" >> "${filev2}"
    printf 'patch "%s.patch" "%s"\n' "${f}" "${f}" >> "${filev3}"
  fi
}

append_remove_instructions() {
  dir="$1"
  filev2="$2"
  filev3="$3"

  if [ -f "$dir/removed-files" ]
  then
    listfile="$dir/removed-files"
  elif [ -f "$dir/Contents/Resources/removed-files" ]
  then
    listfile="$dir/Contents/Resources/removed-files"
  fi
  if [ -z "$listfile" ]; then
    return
  fi

  while read -r f
  do
    # skip blank lines
    if [ -z "$f" ]
    then
      continue
    fi
    # skip comments
    if [[ "$f" =~ ^\#.* ]]
    then
      continue
    fi

    if [[ "$f" =~ .*/$ ]]
    then
      notice "      rmdir \"$f\""
      printf 'rmdir "%s"\n' "${f}" >> "${filev2}"
      printf 'rmdir "%s"\n' "${f}" >> "${filev3}"
    elif [[ "$f" =~ .*/\*$ ]]
    then
      # Remove the *
      f=${f%\*}
      notice "    rmrfdir \"$f\""
      printf 'rmrfdir "%s"\n' "${f}" >> "${filev2}"
      printf 'rmrfdir "%s"\n' "${f}" >> "${filev3}"
    else
      notice "     remove \"$f\""
      printf 'remove "%s"\n' "${f}" >> "${filev2}"
      printf 'remove "%s"\n' "${f}" >> "${filev3}"
    fi
  done <"$listfile"
}

# List all files in the current directory, stripping leading "./"
# Pass a variable name and it will be filled as an array.
list_files() {
  count=0
  tmpfile="$(mktemp temp-filelist.XXXXXX)"
  find . -type f \
    ! -name "update.manifest" \
    ! -name "updatev2.manifest" \
    ! -name "updatev3.manifest" \
    ! -name "temp-dirlist.*" \
    ! -name "temp-filelist.*" \
    | sed 's/\.\/\(.*\)/\1/' \
    | sort -r > "${tmpfile}"
  while read -r file; do
    eval "${1}[$count]=\"$file\""
    (( count++ ))
  done <"${tmpfile}"
  rm -f "${tmpfile}"
}

# List all directories in the current directory, stripping leading "./"
list_dirs() {
  count=0
  tmpfile="$(mktemp temp-dirlist.XXXXXX)"

  find . -type d \
    ! -name "." \
    ! -name ".." \
    | sed 's/\.\/\(.*\)/\1/' \
    | sort -r > "${tmpfile}"
  while read -r dir; do
    eval "${1}[$count]=\"$dir\""
    (( count++ ))
  done <"${tmpfile}"
  rm -f "${tmpfile}"
}
