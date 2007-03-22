#!/bin/bash
#
# This tool generates full update packages for the update system.
# Author: Darin Fisher
#

. $(dirname "$0")/common.sh

# -----------------------------------------------------------------------------

print_usage() {
  notice "Usage: $(basename $0) [OPTIONS] ARCHIVE DIRECTORY"
}

if [ $# = 0 ]; then
  print_usage
  exit 1
fi

if [ $1 = -h ]; then
  print_usage
  notice ""
  notice "The contents of DIRECTORY will be stored in ARCHIVE."
  notice ""
  notice "Options:"
  notice "  -h  show this help text"
  notice ""
  exit 1
fi

# -----------------------------------------------------------------------------

archive="$1"
targetdir="$2"
workdir="$targetdir.work"
manifest="$workdir/update.manifest"
targetfiles="update.manifest"

# Generate a list of all files in the target directory.
pushd "$targetdir"
if test $? -ne 0 ; then
  exit 1
fi

files=($(list_files))

popd

mkdir -p "$workdir"
> $manifest

num_files=${#files[*]}

for ((i=0; $i<$num_files; i=$i+1)); do
  eval "f=${files[$i]}"

  notice "processing $f"

  make_add_instruction "$f" >> $manifest

  dir=$(dirname "$f")
  mkdir -p "$workdir/$dir"
  $BZIP2 -cz9 "$targetdir/$f" > "$workdir/$f"
  copy_perm "$targetdir/$f" "$workdir/$f"

  targetfiles="$targetfiles \"$f\""
done

# Append remove instructions for any dead files.
append_remove_instructions "$targetdir" >> $manifest

$BZIP2 -z9 "$manifest" && mv -f "$manifest.bz2" "$manifest"

eval "$MAR -C \"$workdir\" -c output.mar $targetfiles"
mv -f "$workdir/output.mar" "$archive"

# cleanup
rm -fr "$workdir"
