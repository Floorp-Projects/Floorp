#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Node v10.
# XXX For now, this should match the version installed in
# taskcluster/scripts/misc/repack-node.sh. Later we'll get the ESLint builder
# to use the linux64-node toolchain directly.

wget -O node.xz --progress=dot:mega https://nodejs.org/download/release/v10.19.0/node-v10.19.0-linux-x64.tar.xz

echo '34127c7c6b1ba02d6d4dc3a926f38a5fb88bb37fc7f051349005ce331c7a53c6  node.xz' | sha256sum -c
tar -C /usr/local -xJ --strip-components 1 < node.xz
node -v  # verify
npm -v
