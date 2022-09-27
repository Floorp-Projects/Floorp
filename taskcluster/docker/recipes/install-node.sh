#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Node v12.
# XXX For now, this should match the version installed in
# taskcluster/scripts/misc/repack-node.sh. Later we'll get the ESLint builder
# to use the linux64-node toolchain directly.

wget -O node.xz --progress=dot:mega https://nodejs.org/dist/v12.22.12/node-v12.22.12-linux-x64.tar.xz
echo 'e6d052364bfa2c17da92cf31794100cfd709ba147415ddaeed2222eec9ca1469' node.xz | sha256sum -c
tar -C /usr/local -xJ --strip-components 1 < node.xz
node -v  # verify
npm -v
