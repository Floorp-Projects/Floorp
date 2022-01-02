#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
set -x -e -v

export PATH="$MOZ_FETCHES_DIR/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/rust-makecab

cargo build --verbose --release

mkdir makecab
cp target/release/makecab makecab/
tar caf makecab.tar.zst makecab
mkdir -p $UPLOAD_DIR
cp makecab.tar.zst $UPLOAD_DIR
