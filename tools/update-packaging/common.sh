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
  info=($(ls -l "$1"))
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
  if [ $is_extension = "1" ]; then
    # Use the subdirectory of the extensions folder as the file to test
    # before performing this add instruction.
    testdir=$(echo "$f" | sed 's/\(extensions\/[^\/]*\)\/.*/\1/')
    echo "patch-if \"$testdir\" \"$f.patch\" \"$f\""
  else
    echo "patch \"$f.patch\" \"$f\""
  fi
}

append_remove_instructions() {
  listfile="$1"
  if [ -f "$listfile" ]; then
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
          echo "remove \"$f\""
        else
          notice "ignoring remove instruction for directory: $f"
        fi
      fi
    done
  fi
}

# List all files in the current directory, stripping leading "./"
# Skip the channel-prefs.js file as it should not be included in any
# generated MAR files (see bug 306077).
list_files() {
  find . -type f ! -name "channel-prefs.js" | sed 's/\.\/\(.*\)/"\1"/'
}
