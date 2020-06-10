#!/bin/bash

set -x

source $(dirname $0)/sm-tooltool-config.sh

: ${PYTHON3:=python3}

# Run the script
export MOZ_UPLOAD_DIR="$(cd "$UPLOAD_DIR"; pwd)"
export OBJDIR=$WORK/obj-spider
AUTOMATION=1 $PYTHON3 $GECKO_PATH/js/src/devtools/automation/autospider.py ${SPIDERMONKEY_PLATFORM:+--platform=$SPIDERMONKEY_PLATFORM} $SPIDERMONKEY_VARIANT
BUILD_STATUS=$?

# Ensure upload dir exists
mkdir -p $UPLOAD_DIR

# Copy artifacts for upload by TaskCluster
cp -rL $OBJDIR/dist/bin/{js,jsapi-tests,js-gdb.py} $UPLOAD_DIR

# Fuzzing users want the correct version of llvm-symbolizer available in the
# same directory as the built output.
for f in "$MOZ_FETCHES_DIR/clang/bin/llvm-symbolizer"*; do
    gzip -c "$f" > "$UPLOAD_DIR/llvm-symbolizer.gz" || echo "gzip $f failed" >&2
    break
done

# Fuzzing also uses a few fields in target.json file for automated downloads to
# identify what was built.
if [ -n "$MOZ_BUILD_DATE" ] && [ -n "$GECKO_HEAD_REV" ]; then
    cat >$UPLOAD_DIR/target.json <<EOF
{
  "buildid": "$MOZ_BUILD_DATE",
  "moz_source_stamp": "$GECKO_HEAD_REV"
}
EOF
fi

exit $BUILD_STATUS
