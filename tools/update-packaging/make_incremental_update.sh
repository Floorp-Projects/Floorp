#!/bin/bash
#
# This tool generates incremental update packages for the update system.
# Author: Darin Fisher
#

. $(dirname "$0")/common.sh

# -----------------------------------------------------------------------------

print_usage() {
  notice "Usage: $(basename $0) [OPTIONS] ARCHIVE FROMDIR TODIR"
}

if [ $# = 0 ]; then
  print_usage
  exit 1
fi

if [ $1 = -h ]; then
  print_usage
  notice ""
  notice "The differences between FROMDIR and TODIR will be stored in ARCHIVE."
  notice ""
  notice "Options:"
  notice "  -h  show this help text"
  notice ""
  exit 1
fi

# -----------------------------------------------------------------------------

archive="$1"
olddir="$2"
newdir="$3"
workdir="$newdir.work"
manifest="$workdir/update.manifest"
archivefiles="update.manifest"

# Generate a list of all files in the target directory.
list=$(cd "$olddir" && list_files)
eval "oldfiles=($list)"
list=$(cd "$newdir" && list_files)
eval "newfiles=($list)"

mkdir -p "$workdir"
> $manifest

num_oldfiles=${#oldfiles[*]}

for ((i=0; $i<$num_oldfiles; i=$i+1)); do
  f=${oldfiles[$i]}
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
      if [ $patchsize -lt $fullsize ]; then
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
    if [ "$f" = "${oldfiles[j]}" ]; then
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
append_remove_instructions "$newdir/removed-files" >> $manifest

$BZIP2 -z9 "$manifest" && mv -f "$manifest.bz2" "$manifest"

eval "$MAR -C \"$workdir\" -c output.mar $archivefiles"
mv -f "$workdir/output.mar" "$archive"

# cleanup
rm -fr "$workdir"
