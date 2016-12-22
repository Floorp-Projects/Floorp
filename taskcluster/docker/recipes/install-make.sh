#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs Make 4.0.
tooltool_fetch <<'EOF'
[
  {
    "size": 1872991,
    "visibility": "public",
    "digest": "bc5083937a6cf473be12c0105b2064e546e1765cfc8d3882346cd50e2f3e967acbc5a679b861da07d32dce833d6b55e9c812fe3216cf6db7c4b1f3c232339c88",
    "algorithm": "sha512",
    "filename": "make-4.0.tar.gz",
    "unpack": true
  }
]
EOF
cd make-4.0
./configure
make
make install
# The build system will find `gmake` ahead of `make`, so make sure it finds
# the version we just installed.
ln -s /usr/local/bin/make /usr/local/bin/gmake
cd ..
rm -rf make-4.0
