#!/bin/bash
set -e -v

# This is shared code for building GN.
cd $MOZ_FETCHES_DIR/gn

if test -n "$MAC_CROSS"; then
    python build/gen.py --platform darwin --no-last-commit-position
else
    python build/gen.py --no-last-commit-position
fi

cat > out/last_commit_position.h <<EOF
#ifndef OUT_LAST_COMMIT_POSITION_H_
#define OUT_LAST_COMMIT_POSITION_H_

#define LAST_COMMIT_POSITION_NUM 0
#define LAST_COMMIT_POSITION "unknown"

#endif  // OUT_LAST_COMMIT_POSITION_H_
EOF

ninja -C out -v

STAGE=gn
mkdir -p $UPLOAD_DIR $STAGE

# At this point, the resulting binary is at:
# $WORKSPACE/out/Release/gn
if test "$MAC_CROSS" = "" -a "$(uname)" = "Linux"; then
    strip out/gn
fi
cp out/gn $STAGE

tar -c $STAGE | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > gn.tar.zst
cp gn.tar.zst $UPLOAD_DIR
