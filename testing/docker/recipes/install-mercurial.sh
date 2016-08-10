#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs and configures Mercurial.

set -e

# Detect OS.
if [ -f /etc/lsb-release ]; then
    . /etc/lsb-release

    if [ "${DISTRIB_ID}" = "Ubuntu" -a "${DISTRIB_RELEASE}" = "16.04" ]; then
        HG_DEB=1
        HG_DIGEST=7b1fc1217e0dcaeea852b0af2dc559b1aafb704fbee7e29cbec75af57bacb84910a7ec92b5c33f04ee98f23b3a57f1fa451173fe7c8a96f58faefe319dc7dde1
        HG_SIZE=44878
        HG_FILENAME=mercurial_3.8.4_amd64.deb

        HG_COMMON_DIGEST=b476e2612e7495a1c7c5adfd84511aa7479e26cc9070289513ec705fbfc4c61806ce2dbcceca0e63f2e80669be416f3467a3cebb522dcb8a6aeb62cdd3df82f2
        HG_COMMON_SIZE=1818422
        HG_COMMON_FILENAME=mercurial-common_3.8.4_all.deb
    elif [ "${DISTRIB_ID}" = "Ubuntu" -a "${DISTRIB_RELEASE}" = "12.04" ]; then
        HG_DEB=1
        HG_DIGEST=96366b6baac26017a2ef9cd0bbbba4f18756e02921b0ff3541f677484d53d2041f01202b743ea8cdb96db58e88317da18befefc9711a085b054f9abc3dad1679
        HG_SIZE=54992
        HG_FILENAME=mercurial_3.8.4_amd64.deb

        HG_COMMON_SIZE=2946616
        HG_COMMON_DIGEST=96c6bc305ae85f16885d0b6ac6800361d680811346a01338a56a2174a3c0ae5d86bbd827d93fe8c59d71acd2e1cc7d6e79e39a179836d5695cbfa3b370982ee5
        HG_COMMON_FILENAME=mercurial-common_3.8.4_all.deb
    fi
fi

if [ -n "${HG_DEB}" ]; then
tooltool_fetch <<EOF
[
{
    "size": ${HG_SIZE},
    "digest": "${HG_DIGEST}",
    "algorithm": "sha512",
    "filename": "${HG_FILENAME}"
},
{
    "size": ${HG_COMMON_SIZE},
    "digest": "${HG_COMMON_DIGEST}",
    "algorithm": "sha512",
    "filename": "${HG_COMMON_FILENAME}"
}
]
EOF

    dpkg -i ${HG_COMMON_FILENAME} ${HG_FILENAME}
else
    echo "Do not know how to install Mercurial on this OS"
    exit 1
fi

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
# We also tell progress to assume a TTY is present so updates are printed
# even if there is no known TTY.
[progress]
delay = 1.0
refresh = 1.0
assume-tty = true

[web]
cacerts = /etc/ssl/certs/ca-certificates.crt

[extensions]
robustcheckout = /usr/local/mercurial/robustcheckout.py

# Settings to make 1-click loaners more useful.
[extensions]
color =
histedit =
pager =
rebase =

[diff]
git = 1
showfunc = 1

[pager]
pager = LESS=FRSXQ less

attend-help = true
attend-incoming = true
attend-log = true
attend-outgoing = true
attend-status = true
EOF

chmod 644 /etc/mercurial/hgrc
