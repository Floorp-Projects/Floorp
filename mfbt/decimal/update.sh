# Usage: ./update.sh [blink-core-source-directory]
#
# Copies the needed files from a directory containing the original
# Decimal.h and Decimal.cpp source that we need.
# If [blink-core-source-directory] is not specified, this script will
# attempt to download the latest versions using git.

set -e

FILES=(
  "Decimal.h"
  "Decimal.cpp"
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
  LATEST_SHA=$(git ls-remote https://chromium.googlesource.com/chromium/src.git/ | awk "/refs\/heads\/master/ {print \$1}")
  REPO_PATH="https://chromium.googlesource.com/chromium/src.git/+/$LATEST_SHA/third_party/WebKit/Source/platform"
  #REPO_PATH="https://github.com/WebKit/webkit/tree/master/Source/WebCore/platform"
  for F in "${FILES[@]}"
  do
    printf "Downloading `basename $F`..."
    curl "$REPO_PATH/${F}?format=TEXT" | base64 -D > "$F"
    echo done.
  done
  echo $LATEST_SHA > UPSTREAM-GIT-SHA
fi

# Apply patches:

patch -p3 < zero-serialization.patch
patch -p3 < comparison-with-nan.patch
patch -p3 < mfbt-abi-markers.patch
patch -p3 < to-moz-dependencies.patch
# The following is disabled. See
# https://bugzilla.mozilla.org/show_bug.cgi?id=1208357#c7
#patch -p3 < fix-wshadow-warnings.patch
