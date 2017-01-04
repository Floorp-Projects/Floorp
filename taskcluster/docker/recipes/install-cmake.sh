#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs CMake 3.7.1.
tooltool_fetch <<'EOF'
[
  {
    "size": 7361172,
    "digest": "0539d70ce3ac77042a45d638443b09fbf368e253622db980bc6fb15988743eacd031ab850a45c821ec3e9f0f5f886b9c9cb0668aeda184cd457b78abbfe7b629",
    "algorithm": "sha512",
    "filename": "cmake-3.7.1.tar.gz",
    "unpack": true
  }
]
EOF
cd cmake-3.7.1
./bootstrap && make install
cd ..
rm -rf cmake-3.7.1
