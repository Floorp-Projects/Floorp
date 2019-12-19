#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Node v8.
# XXX For now, this should match the version installed in
# taskcluster/scripts/misc/repack-node.sh. Later we'll get the ESLint builder
# to use the linux64-node toolchain directly.

wget --progress=dot:mega https://nodejs.org/dist/v8.17.0/node-v8.17.0-linux-x64.tar.xz
echo 'b7f6dd77fb173c8c7c30d61d0702eefc236bba74398538aa77bfa2bb47bddce6  node-v8.17.0-linux-x64.tar.xz' | sha256sum -c
tar -C /usr/local -xJ --strip-components 1 < node-v8.17.0-linux-x64.tar.xz
node -v  # verify
npm -v
