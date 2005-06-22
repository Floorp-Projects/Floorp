#!/bin/bash
#
# This tool generates incremental update packages for the update system.
# Author: Darin Fisher
#

# -----------------------------------------------------------------------------
# By default just assume that these tools exist on our path
MAR=${MAR:-mar}
BZIP2=${BZIP2:-bzip2}
MBSDIFF=${MBSDIFF:-mbsdiff}

# -----------------------------------------------------------------------------

print_usage() {
  echo "Usage: $(basename $0) [OPTIONS] ARCHIVE FROMDIR TODIR"
}

if [ $# = 0 ]; then
  print_usage
  exit 1
fi

if [ $1 = -h ]; then
  print_usage
  echo ""
  echo "The differences between FROMDIR and TODIR will be stored in ARCHIVE."
  echo ""
  echo "Options:"
  echo "  -h  show this help text"
  echo ""
  exit 1
fi

# -----------------------------------------------------------------------------

get_file_size() {
  info=($(ls -l "$1"))
  echo ${info[4]}
}

archive="$1"
olddir="$2"
newdir="$3"
workdir="$newdir.work"
manifest="$workdir/update.manifest"
archivefiles="update.manifest"

# Generate a list of all files in the target directory.
list=$(cd "$olddir" && find . -type f | sed 's/\.\/\(.*\)/"\1"/')
eval "oldfiles=($list)"
list=$(cd "$newdir" && find . -type f | sed 's/\.\/\(.*\)/"\1"/')
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
        echo "patch \"$f.patch\" \"$f\"" >> $manifest
        mv -f "$patchfile" "$workdir/$f.patch"
        rm -f "$workdir/$f"
        archivefiles="$archivefiles \"$f.patch\""
      else
        echo "add \"$f\"" >> $manifest
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

  echo "add \"$f\"" >> "$manifest"
  archivefiles="$archivefiles \"$f\""
done

$BZIP2 -z9 "$manifest" && mv -f "$manifest.bz2" "$manifest"

eval "$MAR -C \"$workdir\" -c output.mar $archivefiles"
mv -f "$workdir/output.mar" "$archive"

# cleanup
rm -fr "$workdir"
