#!/bin/sh

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
project=${artifact%.tar.*}

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

cd $MOZ_FETCHES_DIR
mv host-utils-* $project
tar -acvf $artifact $project
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR
