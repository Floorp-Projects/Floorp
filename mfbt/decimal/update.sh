# Usage: ./update.sh [blink-core-source-directory]
#
# Copies the needed files from a directory containing the original
# Decimal.h and Decimal.cpp source that we need.
# If [blink-core-source-directory] is not specified, this script will
# attempt to download the latest versions using svn.

# This was last updated with svn r148833

set -e

FILES=(
  "LICENSE-APPLE"
  "LICENSE-LGPL-2"
  "LICENSE-LGPL-2.1"
  "platform/Decimal.h"
  "platform/Decimal.cpp"
)

OWN_NAME=`basename $0`

if [ $# -gt 1 ]; then
  echo "$OWN_NAME: Too many arguments">&2
  exit 1
fi

if [ $# -eq 1 ]; then
  BLINK_CORE_DIR="$1"
  for F in "${FILES[@]}"
  do
    P="$BLINK_CORE_DIR/$F"
    if [ ! -f "$P" ]; then
      echo "$OWN_NAME: Couldn't find file: $P">&2
      exit 1
    fi
  done
  for F in "${FILES[@]}"
  do
    P="$BLINK_CORE_DIR/$F"
    cp "$P" .
  done
else
  SVN="svn --non-interactive --trust-server-cert"
  REPO_PATH="https://src.chromium.org/blink/trunk/Source/core"
  #REPO_PATH="https://svn.webkit.org/repository/webkit/trunk/Source/WebCore"

  printf "Looking up latest Blink revision number..."
  LATEST_REV=`$SVN info $REPO_PATH | grep '^Revision: ' | cut -c11-`
  echo done.

  for F in "${FILES[@]}"
  do
    printf "Exporting r$LATEST_REV of `basename $F`..."
    $SVN export -r $LATEST_REV $REPO_PATH/$F 2>/dev/null 1>&2
    echo done.
  done
fi

# Apply patches:

patch -p3 < floor-ceiling.patch
patch -p3 < zero-serialization.patch
patch -p3 < comparison-with-nan.patch
patch -p3 < mfbt-abi-markers.patch
patch -p3 < to-moz-dependencies.patch
patch -p3 < fix-wshadow-warnings.patch
