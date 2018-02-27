#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This tool generates incremental update packages for the update system.
# Author: Darin Fisher
#

# shellcheck disable=SC1090
. "$(dirname "$0")/common.sh"

# -----------------------------------------------------------------------------

print_usage() {
  notice "Usage: $(basename "$0") [OPTIONS] ARCHIVE FROMDIR TODIR"
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

  local f

  if [ "$forced_file_chk" = "precomplete" ]; then
    return 0;
  fi

  if [ "$forced_file_chk" = "Contents/Resources/precomplete" ]; then
    return 0;
  fi

  if [ "$forced_file_chk" = "removed-files" ]; then
    return 0;
  fi

  if [ "$forced_file_chk" = "Contents/Resources/removed-files" ]; then
    return 0;
  fi

  if [ "$forced_file_chk" = "chrome.manifest" ]; then
    return 0;
  fi

  if [ "$forced_file_chk" = "Contents/Resources/chrome.manifest" ]; then
    return 0;
  fi

  if [ "${forced_file_chk##*.}" = "chk" ]; then
    return 0;
  fi

  for f in $force_list; do
    #echo comparing $forced_file_chk to $f
    if [ "$forced_file_chk" = "$f" ]; then
      return 0;
    fi
  done
  return 1;
}

if [ $# = 0 ]; then
  print_usage
  exit 1
fi

requested_forced_updates='Contents/MacOS/firefox'

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

arg_start=$((OPTIND-1))
shift $arg_start

archive="$1"
olddir="$2"
newdir="$3"
# Prevent the workdir from being inside the targetdir so it isn't included in
# the update mar.

# Remove the /
newdir="${newdir%/}"

workdir="$newdir.work"
updatemanifestv2="$workdir/updatev2.manifest"
updatemanifestv3="$workdir/updatev3.manifest"
archivefiles="updatev2.manifest updatev3.manifest"

mkdir -p "$workdir"

# Generate a list of all files in the target directory.
pushd "$olddir" || exit 1

declare -a oldfiles
list_files oldfiles
declare -a olddirs
list_dirs olddirs

popd || exit 1

pushd "$newdir" || exit 1

if [ ! -f "precomplete" ] && [ ! -f "Contents/Resources/precomplete" ]
then
  notice "precomplete file is missing!"
  exit 1
fi

declare -a newfiles
list_files newfiles

popd || exit 1

# Add the type of update to the beginning of the update manifests.
notice ""
notice "Adding type instruction to update manifests"
: > "$updatemanifestv2"
: > "$updatemanifestv3"
notice "       type partial"
echo 'type "partial"' >> "$updatemanifestv2"
echo 'type "partial"' >> "$updatemanifestv3"

notice ""
notice "Adding file patch and add instructions to update manifests"

declare -a remove_array
num_removes=0

for f in "${oldfiles[@]}"
do
  # If this file exists in the new directory as well, then check if it differs.
  if [ -f "$newdir/$f" ]; then

    if check_for_add_if_not_update "$f"; then
      # The full workdir may not exist yet, so create it if necessary.
      mkdir -p "$(dirname "$workdir/$f")"
      if [[ -n $MAR_OLD_FORMAT ]]; then
        $BZIP2 -cz9 "$newdir/$f" > "$workdir/$f"
      else
        $XZ --compress --x86 --lzma2 --format=xz --check=crc64 --force --stdout "$newdir/$f" > "$workdir/$f"
      fi
      copy_perm "$newdir/$f" "$workdir/$f"
      make_add_if_not_instruction "$f" "$updatemanifestv3"
      archivefiles="$archivefiles \"$f\""
      continue 1
    fi

    if check_for_forced_update "$requested_forced_updates" "$f"; then
      # The full workdir may not exist yet, so create it if necessary.
      mkdir -p "$(dirname "$workdir/$f")"
      if [[ -n $MAR_OLD_FORMAT ]]; then
        $BZIP2 -cz9 "$newdir/$f" > "$workdir/$f"
      else
        $XZ --compress --x86 --lzma2 --format=xz --check=crc64 --force --stdout "$newdir/$f" > "$workdir/$f"
      fi
      copy_perm "$newdir/$f" "$workdir/$f"
      make_add_instruction "$f" "$updatemanifestv2" "$updatemanifestv3" 1
      archivefiles="$archivefiles \"$f\""
      continue 1
    fi

    if ! diff "$olddir/$f" "$newdir/$f" > /dev/null; then
      # Compute both the compressed binary diff and the compressed file, and
      # compare the sizes.  Then choose the smaller of the two to package.
      dir=$(dirname "$workdir/$f")
      mkdir -p "$dir"
      notice "diffing \"$f\""
      # MBSDIFF_HOOK represents the communication interface with funsize and,
      # if enabled, caches the intermediate patches for future use and
      # compute avoidance
      #
      # An example of MBSDIFF_HOOK env variable could look like this:
      # export MBSDIFF_HOOK="myscript.sh -A https://funsize/api -c /home/user"
      # where myscript.sh has the following usage:
      # myscript.sh -A SERVER-URL [-c LOCAL-CACHE-DIR-PATH] [-g] [-u] \
      #   PATH-FROM-URL PATH-TO-URL PATH-PATCH SERVER-URL
      #
      # Note: patches are bzipped or xz stashed in funsize to gain more speed

      if [[ -n $MAR_OLD_FORMAT ]]
      then
        COMPRESS="$BZIP2 -z9"
        EXT=".bz2"
        STDOUT="-c"
      else
        COMPRESS="$XZ --compress --x86 --lzma2 --format=xz --check=crc64 --force"
        EXT=".xz"
        STDOUT="--stdout"
      fi

      if [ -z "$MBSDIFF_HOOK" ]
      then
        $MBSDIFF "$olddir/$f" "$newdir/$f" "$workdir/$f.patch"
        $COMPRESS "$workdir/$f.patch"
      else
        if $MBSDIFF_HOOK -g "$olddir/$f" "$newdir/$f" "$workdir/$f.patch${EXT}"
        then
          notice "file \"$f\" found in funsize, diffing skipped"
        else
          # if not found already - compute it and cache it for future use
          $MBSDIFF "$olddir/$f" "$newdir/$f" "$workdir/$f.patch"
          $COMPRESS "$workdir/$f.patch"
          $MBSDIFF_HOOK -u "$olddir/$f" "$newdir/$f" "$workdir/$f.patch${EXT}"
        fi
      fi
      $COMPRESS $STDOUT "$newdir/$f" > "$workdir/$f"

      copy_perm "$newdir/$f" "$workdir/$f"
      patchfile="$workdir/$f.patch${EXT}"

      patchsize=$(get_file_size "$patchfile")
      fullsize=$(get_file_size "$workdir/$f")

      if [ "$patchsize" -lt "$fullsize" ]; then
        make_patch_instruction "$f" "$updatemanifestv2" "$updatemanifestv3"
        mv -f "$patchfile" "$workdir/$f.patch"
        rm -f "$workdir/$f"
        archivefiles="$archivefiles \"$f.patch\""
      else
        make_add_instruction "$f" "$updatemanifestv2" "$updatemanifestv3"
        rm -f "$patchfile"
        archivefiles="$archivefiles \"$f\""
      fi
    fi
  else
    # remove instructions are added after add / patch instructions for
    # consistency with make_incremental_updates.py
    remove_array[$num_removes]=$f
    (( num_removes++ ))
  fi
done

# Newly added files
notice ""
notice "Adding file add instructions to update manifests"

for f in "${newfiles[@]}"
do
  for j in "${oldfiles[@]}"
  do
    if [ "$f" = "$j" ]
    then
      continue 2
    fi
  done
  dir=$(dirname "$workdir/$f")
  mkdir -p "$dir"

  if [[ -n $MAR_OLD_FORMAT ]]; then
    $BZIP2 -cz9 "$newdir/$f" > "$workdir/$f"
  else
    $XZ --compress --x86 --lzma2 --format=xz --check=crc64 --force --stdout "$newdir/$f" > "$workdir/$f"
  fi
  copy_perm "$newdir/$f" "$workdir/$f"

  if check_for_add_if_not_update "$f"; then
    make_add_if_not_instruction "$f" "$updatemanifestv3"
  else
    make_add_instruction "$f" "$updatemanifestv2" "$updatemanifestv3"
  fi


  archivefiles="$archivefiles \"$f\""
done

notice ""
notice "Adding file remove instructions to update manifests"
for f in "${remove_array[@]}"
do
  notice "     remove \"$f\""
  echo "remove \"$f\"" >> "$updatemanifestv2"
  echo "remove \"$f\"" >> "$updatemanifestv3"
done

# Add remove instructions for any dead files.
notice ""
notice "Adding file and directory remove instructions from file 'removed-files'"
append_remove_instructions "$newdir" "$updatemanifestv2" "$updatemanifestv3"

notice ""
notice "Adding directory remove instructions for directories that no longer exist"

for f in "${olddirs[@]}"
do
  # If this dir doesn't exist in the new directory remove it.
  if [ ! -d "$newdir/$f" ]; then
    notice "      rmdir $f/"
    echo "rmdir \"$f/\"" >> "$updatemanifestv2"
    echo "rmdir \"$f/\"" >> "$updatemanifestv3"
  fi
done

if [[ -n $MAR_OLD_FORMAT ]]; then
  $BZIP2 -z9 "$updatemanifestv2" && mv -f "$updatemanifestv2.bz2" "$updatemanifestv2"
  $BZIP2 -z9 "$updatemanifestv3" && mv -f "$updatemanifestv3.bz2" "$updatemanifestv3"
else
  $XZ --compress --x86 --lzma2 --format=xz --check=crc64 --force "$updatemanifestv2" && mv -f "$updatemanifestv2.xz" "$updatemanifestv2"
  $XZ --compress --x86 --lzma2 --format=xz --check=crc64 --force "$updatemanifestv3" && mv -f "$updatemanifestv3.xz" "$updatemanifestv3"
fi

mar_command="$MAR"
if [[ -n $MOZ_PRODUCT_VERSION ]]
then
  mar_command="$mar_command -V $MOZ_PRODUCT_VERSION"
fi
if [[ -n $MOZ_CHANNEL_ID ]]
then
  mar_command="$mar_command -H $MOZ_CHANNEL_ID"
fi
mar_command="$mar_command -C \"$workdir\" -c output.mar"
eval "$mar_command $archivefiles"
mv -f "$workdir/output.mar" "$archive"

# cleanup
rm -fr "$workdir"

notice ""
notice "Finished"
notice ""
