#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs and configures Mercurial.

set -e

# ASSERTION: We are running Ubuntu 16.04.
tooltool_fetch <<EOF
[
{
    "size": 44878,
    "digest": "7b1fc1217e0dcaeea852b0af2dc559b1aafb704fbee7e29cbec75af57bacb84910a7ec92b5c33f04ee98f23b3a57f1fa451173fe7c8a96f58faefe319dc7dde1",
    "algorithm": "sha512",
    "filename": "mercurial_3.8.4_amd64.deb"
},
{
    "size": 1818422,
    "digest": "b476e2612e7495a1c7c5adfd84511aa7479e26cc9070289513ec705fbfc4c61806ce2dbcceca0e63f2e80669be416f3467a3cebb522dcb8a6aeb62cdd3df82f2",
    "algorithm": "sha512",
    "filename": "mercurial-common_3.8.4_all.deb"
}
]
EOF

dpkg -i mercurial-common_3.8.4_all.deb mercurial_3.8.4_amd64.deb

mkdir -p /usr/local/mercurial
cd /usr/local/mercurial
tooltool_fetch <<'EOF'
[
{
    "size": 11849,
    "digest": "c88d9b8afd6649bd28bbacfa654ebefec8087a01d1662004aae088d485edeb03a92df1193d1310c0369d7721f475b974fcd4a911428ec65936f7e40cf1609c49",
    "algorithm": "sha512",
    "filename": "robustcheckout.py"
}
]
EOF
chmod 644 /usr/local/mercurial/robustcheckout.py

mkdir -p /etc/mercurial
cat >/etc/mercurial/hgrc <<EOF
# By default the progress bar starts after 3s and updates every 0.1s. We
# change this so it shows and updates every 1.0s.
[progress]
delay = 1.0
refresh = 1.0

[web]
cacerts = /etc/ssl/certs/ca-certificates.crt

[extensions]
robustcheckout = /usr/local/mercurial/robustcheckout.py
EOF

chmod 644 /etc/mercurial/hgrc
