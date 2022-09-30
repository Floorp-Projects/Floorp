#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Node v16.
# XXX For now, this should match the version installed in
# taskcluster/scripts/misc/repack-node.sh. Later we'll get the ESLint builder
# to use the linux64-node toolchain directly.

wget -O node.xz --progress=dot:mega https://nodejs.org/dist/v16.17.1/node-v16.17.1-linux-x64.tar.xz
echo '06ba2eb34aa385967f5f58c87a44753f83212f6cccea892b33f80a2e7fda8384' node.xz | sha256sum -c
tar -C /usr/local -xJ --strip-components 1 < node.xz
node -v  # verify
npm -v
