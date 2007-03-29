#!/bin/bash
#
# This tool generates incremental update packages for the update system.
# Author: Darin Fisher
#

. $(dirname "$0")/common.sh

# -----------------------------------------------------------------------------

print_usage() {
  notice "Usage: $(basename $0) [OPTIONS] ARCHIVE FROMDIR TODIR"
  notice ""
  notice "The differences between FROMDIR and TODIR will be stored in ARCHIVE."
  notice ""
  notice "Options:"
  notice "  -h  show this help text"
  notice "  -f  clobber this file in the installation"
  notice "      Must be a path to a file to clobber in the partial update."
  notice ""
}

check_for_forced_update() {
  force_list="$1"
  forced_file_chk="$2"

  ## 'false'... because this is bash. Oh yay!  
  local do_force=1
  local f

  for f in $force_list; do
    #echo comparing $forced_file_chk to $f
    if [ "$forced_file_chk" = "$f" ]; then
      ## "true" *giggle*
      do_force=0
      break
    fi
  done
  return $do_force;
}

if [ $# = 0 ]; then
  print_usage
  exit 1
fi

requested_forced_updates=''

while getopts "hf:" flag
do
   case "$flag" in
      h) print_usage; exit 0
      ;;
      f) requested_forced_updates="$requested_forced_updates $OPTARG"
      ;;
      ?) print_usage; exit 1
      ;;
   esac
done

# -----------------------------------------------------------------------------

let arg_start=$OPTIND-1
shift $arg_start

archive="$1"
olddir="$2"
newdir="$3"
workdir="$newdir.work"
manifest="$workdir/update.manifest"
archivefiles="update.manifest"

mkdir -p "$workdir"

# Generate a list of all files in the target directory.
pushd "$olddir"
if test $? -ne 0 ; then
  exit 1
fi

list_files oldfiles

popd

pushd "$newdir"
if test $? -ne 0 ; then
  exit 1
fi

list_files newfiles

popd

> $manifest

num_oldfiles=${#oldfiles[*]}

for ((i=0; $i<$num_oldfiles; i=$i+1)); do
  f="${oldfiles[$i]}"

  # This file is created by Talkback, so we can ignore it
  if [ "$f" = "readme.txt" ]; then
    continue 1
  fi

  # If this file exists in the new directory as well, then check if it differs.
  if [ -f "$newdir/$f" ]; then
    if ! diff "$olddir/$f" "$newdir/$f" > /dev/null; then
      # Compute both the compressed binary diff and the compressed file, and
      # compare the sizes.  Then choose the smaller of the two to package.
      echo "  diffing $f"
      dir=$(dirname "$workdir/$f")
      mkdir -p "$dir"
      $MBSDIFF "$olddir/$f" "$newdir/$f" "$workdir/$f.patch"
      $BZIP2 -z9 "$workdir/$f.patch"
      $BZIP2 -cz9 "$newdir/$f" > "$workdir/$f"
      patchfile="$workdir/$f.patch.bz2"
      patchsize=$(get_file_size "$patchfile")
      fullsize=$(get_file_size "$workdir/$f")

      if check_for_forced_update "$requested_forced_updates" "$f"; then
        echo 1>&2 "  FORCING UPDATE for file '$f'..."
        make_add_instruction "$f" >> $manifest
        rm -f "$patchfile"
        archivefiles="$archivefiles \"$f\""
        continue 1
      fi

      if [ $patchsize -lt $fullsize -a "$f" != "removed-files" ]; then
        make_patch_instruction "$f" >> $manifest
        mv -f "$patchfile" "$workdir/$f.patch"
        rm -f "$workdir/$f"
        archivefiles="$archivefiles \"$f.patch\""
      else
        make_add_instruction "$f" >> $manifest
        rm -f "$patchfile"
        archivefiles="$archivefiles \"$f\""
      fi
    fi
  else
    echo "remove \"$f\"" >> $manifest
  fi
done

# Now, we just need to worry about newly added files
num_newfiles=${#newfiles[*]}

for ((i=0; $i<$num_newfiles; i=$i+1)); do
  f="${newfiles[$i]}"

  # If we've already tested this file, then skip it
  for ((j=0; $j<$num_oldfiles; j=$j+1)); do
    if [ "\"$f\"" = "${oldfiles[j]}" ]; then
      continue 2
    fi
  done
  
  dir=$(dirname "$workdir/$f")
  mkdir -p "$dir"

  $BZIP2 -cz9 "$newdir/$f" > "$workdir/$f"

  make_add_instruction "$f" >> "$manifest"
  archivefiles="$archivefiles \"$f\""
done

# Append remove instructions for any dead files.
append_remove_instructions "$newdir" >> $manifest

$BZIP2 -z9 "$manifest" && mv -f "$manifest.bz2" "$manifest"

eval "$MAR -C \"$workdir\" -c output.mar $archivefiles"
mv -f "$workdir/output.mar" "$archive"

# cleanup
rm -fr "$workdir"
