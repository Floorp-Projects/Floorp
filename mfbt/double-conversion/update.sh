# Usage: ./update.sh [<git-rev-to-use>]
#
# Copies the needed files from a directory containing the original
# double-conversion source that we need.  If no revision is specified, the tip
# revision is used.

# This was last updated with git rev 04cae7a8d5ef3d62ceffb03cdc3d38f258457a52.

set -e

TMPDIR=`mktemp --directory`
LOCAL_CLONE="$TMPDIR/double-conversion"

git clone https://github.com/google/double-conversion.git "$LOCAL_CLONE"

REV=""

if [ "$1" !=  "" ]; then
  git -C "$LOCAL_CLONE" checkout "$1"
fi

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

patch -p3 < add-mfbt-api-markers.patch
patch -p3 < use-StandardInteger.patch
patch -p3 < use-mozilla-assertions.patch
patch -p3 < use-static_assert.patch
patch -p3 < ToPrecision-exponential.patch
patch -p3 < fix-Wshadow-issues.patch
