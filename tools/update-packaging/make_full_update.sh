#!/bin/bash
#
# This tool generates full update packages for the update system.
# Author: Darin Fisher
#

# -----------------------------------------------------------------------------
# By default just assume that these tools exist on our path
MAR=${MAR:-mar}
BZIP2=${BZIP2:-bzip2}

# -----------------------------------------------------------------------------

print_usage() {
  echo "Usage: $(basename $0) [OPTIONS] ARCHIVE DIRECTORY"
}

if [ $# = 0 ]; then
  print_usage
  exit 1
fi

if [ $1 = -h ]; then
  print_usage
  echo ""
  echo "The contents of DIRECTORY will be stored in ARCHIVE."
  echo ""
  echo "Options:"
  echo "  -h  show this help text"
  echo ""
  exit 1
fi

# -----------------------------------------------------------------------------

copy_perm() {
  reference="$1"
  target="$2"

  if [ -x "$reference" ]; then
    chmod 0755 "$target"
  else
    chmod 0644 "$target"
  fi
}

archive="$1"
targetdir="$2"
workdir="$targetdir.work"
manifest="$workdir/update.manifest"
targetfiles="update.manifest"

# Generate a list of all files in the target directory.
list=$(cd "$targetdir" && find . -type f | sed 's/\.\/\(.*\)/"\1"/')
eval "files=($list)"

mkdir -p "$workdir"
> $manifest

num_files=${#files[*]}

for ((i=0; $i<$num_files; i=$i+1)); do
  f=${files[$i]}

  echo "  processing $f"

  echo "add \"$f\"" >> $manifest

  dir=$(dirname "$f")
  mkdir -p "$workdir/$dir"
  $BZIP2 -cz9 "$targetdir/$f" > "$workdir/$f"
  copy_perm "$targetdir/$f" "$workdir/$f"

  targetfiles="$targetfiles \"$f\""
done

$BZIP2 -z9 "$manifest" && mv -f "$manifest.bz2" "$manifest"

eval "$MAR -C \"$workdir\" -c output.mar $targetfiles"
mv -f "$workdir/output.mar" "$archive"

# cleanup
rm -fr "$workdir"
