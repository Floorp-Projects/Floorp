# Usage: ./update.sh [<git-rev-to-use>]
#
# Copies the needed files from a directory containing the original
# double-conversion source that we need.  If no revision is specified, the tip
# revision is used.

# This was last updated with git rev d8d4e668ee1e6e10b728f0671a89b07d7c4d45be.

set -e

LOCAL_PATCHES=""

LOCAL_PATCHES="$LOCAL_PATCHES add-mfbt-api-markers.patch"
LOCAL_PATCHES="$LOCAL_PATCHES use-StandardInteger.patch"
LOCAL_PATCHES="$LOCAL_PATCHES use-mozilla-assertions.patch"
LOCAL_PATCHES="$LOCAL_PATCHES ToPrecision-exponential.patch"

TMPDIR=`mktemp --directory`
LOCAL_CLONE="$TMPDIR/double-conversion"

git clone https://github.com/google/double-conversion.git "$LOCAL_CLONE"

REV=""

if [ "$1" !=  "" ]; then
  git -C "$LOCAL_CLONE" checkout "$1"
fi

# First clear out everything already present.
DEST=./source
rm -rf "$DEST"
mkdir "$DEST"

# Copy over critical files.
cp "$LOCAL_CLONE/LICENSE" "$DEST/"
cp "$LOCAL_CLONE/README.md" "$DEST/"

# Includes
for header in "$LOCAL_CLONE/double-conversion/"*.h; do
  cp "$header" "$DEST/"
done

# Source
for ccfile in "$LOCAL_CLONE/double-conversion/"*.cc; do
  cp "$ccfile" "$DEST/"
done

# Now apply our local patches.
for patch in $LOCAL_PATCHES; do
  patch --directory "$DEST" --strip 4 < "$patch"
done

# Update Mercurial file status.
hg addremove "$DEST"
