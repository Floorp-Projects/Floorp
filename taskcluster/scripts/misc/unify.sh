#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -x -e

export LIPO=$MOZ_FETCHES_DIR/cctools/bin/x86_64-apple-darwin-lipo

for i in x64 aarch64; do
    $GECKO_PATH/mach python -m mozbuild.action.unpack_dmg $MOZ_FETCHES_DIR/$i/target.dmg $i
done
$GECKO_PATH/mach python $GECKO_PATH/toolkit/mozapps/installer/unify.py x64/*.app aarch64/*.app
$GECKO_PATH/mach python -m mozbuild.action.make_dmg x64 target.dmg

mkdir -p $UPLOAD_DIR
mv target.dmg $UPLOAD_DIR/

python3 -c '
import json
import os

for artifact in json.loads(os.environ["MOZ_FETCHES"]):
    if artifact.get("extract") and artifact.get("dest", "").startswith("x64"):
        print(artifact["dest"], os.path.basename(artifact["artifact"]))
' | while read dir artifact; do
    if [ "$artifact" = target.crashreporter-symbols.zip ]; then
        $GECKO_PATH/mach python $GECKO_PATH/python/mozbuild/mozbuild/action/unify_symbols.py $MOZ_FETCHES_DIR/$dir $MOZ_FETCHES_DIR/aarch64${dir#x64}
    else
        $GECKO_PATH/mach python $GECKO_PATH/python/mozbuild/mozbuild/action/unify_tests.py $MOZ_FETCHES_DIR/$dir $MOZ_FETCHES_DIR/aarch64${dir#x64}
    fi

    case $artifact in
    *.tar.gz)
        find $MOZ_FETCHES_DIR/$dir -not -type d -printf '%P\0' | tar -C $MOZ_FETCHES_DIR/$dir --owner=0:0 --group=0:0 -zcf $artifact --no-recursion --null -T -
        ;;
    *.zip)
        $GECKO_PATH/mach python $GECKO_PATH/python/mozbuild/mozbuild/action/zip.py -C $MOZ_FETCHES_DIR/$dir $PWD/$artifact '*'
        ;;
    esac
    mv $artifact $UPLOAD_DIR/
done
