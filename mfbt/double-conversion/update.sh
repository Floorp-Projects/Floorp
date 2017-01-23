# Usage: ./update.sh [<git-rev-to-use>]
#
# Copies the needed files from a directory containing the original
# double-conversion source that we need.  If no revision is specified, the tip
# revision is used.

# This was last updated with git rev 04cae7a8d5ef3d62ceffb03cdc3d38f258457a52.

set -e

LOCAL_PATCHES=""

LOCAL_PATCHES="$LOCAL_PATCHES add-mfbt-api-markers.patch"
LOCAL_PATCHES="$LOCAL_PATCHES use-StandardInteger.patch"
LOCAL_PATCHES="$LOCAL_PATCHES use-mozilla-assertions.patch"
LOCAL_PATCHES="$LOCAL_PATCHES use-static_assert.patch"
LOCAL_PATCHES="$LOCAL_PATCHES ToPrecision-exponential.patch"
LOCAL_PATCHES="$LOCAL_PATCHES fix-Wshadow-issues.patch"

TMPDIR=`mktemp --directory`
LOCAL_CLONE="$TMPDIR/double-conversion"

git clone https://github.com/google/double-conversion.git "$LOCAL_CLONE"

REV=""

if [ "$1" !=  "" ]; then
  git -C "$LOCAL_CLONE" checkout "$1"
fi

# First clear out everything already present.
rm -rf ./*

# Restore non-upstream files
hg revert update.sh
hg revert $LOCAL_PATCHES

# Copy over critical files.
cp "$LOCAL_CLONE/LICENSE" ./
cp "$LOCAL_CLONE/README" ./

# Includes
for header in "$LOCAL_CLONE/src/"*.h; do
  cp "$header" ./
done

# Source
for ccfile in "$LOCAL_CLONE/src/"*.cc; do
  cp "$ccfile" ./
done

# Now apply our local patches.
for patch in $LOCAL_PATCHES; do
  patch -p3 < "$patch"
done
