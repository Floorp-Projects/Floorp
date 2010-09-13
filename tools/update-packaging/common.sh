#!/bin/bash
#
# Code shared by update packaging scripts.
# Author: Darin Fisher
#

# -----------------------------------------------------------------------------
# By default just assume that these tools exist on our path
MAR=${MAR:-mar}
BZIP2=${BZIP2:-bzip2}
MBSDIFF=${MBSDIFF:-mbsdiff}

# -----------------------------------------------------------------------------
# Helper routines

notice() {
  echo $* 1>&2
}

get_file_size() {
  info=($(ls -ln "$1"))
  echo ${info[4]}
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
  is_extension=$(echo "$f" | grep -c 'extensions/.*/')
  if [ $is_extension = "1" ]; then
    # Use the subdirectory of the extensions folder as the file to test
    # before performing this add instruction.
    testdir=$(echo "$f" | sed 's/\(extensions\/[^\/]*\)\/.*/\1/')
    echo "add-if \"$testdir\" \"$f\""
  else
    echo "add \"$f\""
  fi
}

make_patch_instruction() {
  f="$1"
  is_extension=$(echo "$f" | grep -c 'extensions/.*/')
  is_search_plugin=$(echo "$f" | grep -c 'searchplugins/.*')
  if [ $is_extension = "1" ]; then
    # Use the subdirectory of the extensions folder as the file to test
    # before performing this add instruction.
    testdir=$(echo "$f" | sed 's/\(extensions\/[^\/]*\)\/.*/\1/')
    echo "patch-if \"$testdir\" \"$f.patch\" \"$f\""
  elif [ $is_search_plugin = "1" ]; then
    echo "patch-if \"$f\" \"$f.patch\" \"$f\""
  else
    echo "patch \"$f.patch\" \"$f\""
  fi
}

append_remove_instructions() {
  dir="$1"
  if [ -f "$dir/removed-files" ]; then
    prefix=
    listfile="$dir/removed-files"
  elif [ -f "$dir/Contents/MacOS/removed-files" ]; then
    prefix=Contents/MacOS/
    listfile="$dir/Contents/MacOS/removed-files"
  fi
  if [ -n "$listfile" ]; then
    # Map spaces to pipes so that we correctly handle filenames with spaces.
    files=($(cat "$listfile" | tr " " "|"))  
    num_files=${#files[*]}
    for ((i=0; $i<$num_files; i=$i+1)); do
      # Trim whitespace (including trailing carriage returns)
      f=$(echo ${files[$i]} | tr "|" " " | sed 's/^ *\(.*\) *$/\1/' | tr -d '\r')
      # Exclude any blank lines or any lines ending with a slash, which indicate
      # directories.  The updater doesn't know how to remove entire directories.
      if [ -n "$f" ]; then
        if [ $(echo "$f" | grep -c '\/$') = 0 ]; then
          echo "remove \"$prefix$f\""
        else
          notice "ignoring remove instruction for directory: $f"
        fi
      fi
    done
  fi
}

# List all files in the current directory, stripping leading "./"
# Skip the channel-prefs.js file as it should not be included in any
# generated MAR files (see bug 306077). Pass a variable name and it will be
# filled as an array.
list_files() {
  count=0

  find . -type f \
    ! -name "channel-prefs.js" \
    ! -name "update.manifest" \
    ! -name "temp-filelist" \
    | sed 's/\.\/\(.*\)/\1/' \
    | sort > "temp-filelist"
  while read file; do
    eval "${1}[$count]=\"$file\""
    (( count++ ))
  done < "temp-filelist"
  rm "temp-filelist"
}
