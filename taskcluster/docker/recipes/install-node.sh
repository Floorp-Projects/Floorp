#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Node v6.

wget --progress=dot:mega https://nodejs.org/dist/v6.9.1/node-v6.9.1-linux-x64.tar.gz
echo 'a9d9e6308931fa2a2b0cada070516d45b76d752430c31c9198933c78f8d54b17  node-v6.9.1-linux-x64.tar.gz' | sha256sum -c
tar -C /usr/local -xz --strip-components 1 < node-v6.9.1-linux-x64.tar.gz
node -v  # verify
npm -v
