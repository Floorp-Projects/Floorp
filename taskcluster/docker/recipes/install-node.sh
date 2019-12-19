#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Node v8.
# XXX For now, this should match the version installed in
# taskcluster/scripts/misc/repack-node.sh. Later we'll get the ESLint builder
# to use the linux64-node toolchain directly.

wget --progress=dot:mega https://nodejs.org/dist/v8.11.3/node-v8.11.3-linux-x64.tar.xz
echo '08e2fcfea66746bd966ea3a89f26851f1238d96f86c33eaf6274f67fce58421a  node-v8.11.3-linux-x64.tar.xz' | sha256sum -c
tar -C /usr/local -xJ --strip-components 1 < node-v8.11.3-linux-x64.tar.xz
node -v  # verify
npm -v
