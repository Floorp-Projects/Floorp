#!/bin/bash
set -x

# Because we specify a private artifact, UPLOAD_DIR is not populated for us
export UPLOAD_DIR="/builds/worker/private-artifacts/"

# # Delete the external directory
rm -rf $GECKO_PATH/build/clang-plugin/external/*

# Move external repository into its place
cp -r $MOZ_FETCHES_DIR/civet.git/* $GECKO_PATH/build/clang-plugin/external

# Call build-clang.sh with this script's first argument (our JSON config)
$GECKO_PATH/taskcluster/scripts/misc/build-clang.sh $1
