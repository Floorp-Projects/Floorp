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

# Copy artifacts for upload by TaskCluster.
upload=${MOZ_AUTOMATION_UPLOAD-1}
# User-provided override switch.
if [ -n "$MOZ_JS_UPLOAD_BINARIES" ]; then
    upload=1
fi
if [ "$upload" = "1" ]; then
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
else # !upload

# Provide a note for users on why we don't include artifacts for these builds
# by default, and how they can get the artifacts if they really need them.
cat >$UPLOAD_DIR/README-artifacts.txt <<EOF
Artifact upload has been disabled for this build due to infrequent usage of the
generated artifacts.  If you find yourself in a position where you need the
shell or similar artifacts from this build, please redo your push with the
environment variable MOZ_JS_UPLOAD_BINARIES set to 1.  You can provide this as
the option `--env MOZ_JS_UPLOAD_BINARIES=1` to `mach try fuzzy` or `mach try auto`.
EOF
fi

exit $BUILD_STATUS
