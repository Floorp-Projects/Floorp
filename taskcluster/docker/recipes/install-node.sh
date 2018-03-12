#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Node v8.

wget --progress=dot:mega https://nodejs.org/dist/v8.9.4/node-v8.9.4-linux-x64.tar.gz
echo '21fb4690e349f82d708ae766def01d7fec1b085ce1f5ab30d9bda8ee126ca8fc  node-v8.9.4-linux-x64.tar.gz' | sha256sum -c
tar -C /usr/local -xz --strip-components 1 < node-v8.9.4-linux-x64.tar.gz
node -v  # verify
npm -v
