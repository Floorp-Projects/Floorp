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
        HG_DIGEST=09c7c80324158b755c23855d47caeb40b953218b1c89c7f5f21bbdea9de1d13a7ed5a7e647022ff626fb9674655baf05f6965361ccef3fa82b94d1fa8a684187
        HG_SIZE=44956
        HG_FILENAME=mercurial_3.9.1_amd64.deb

        HG_COMMON_DIGEST=ef281d1368a8cf2363bc08c050aff3825028ba4d47d491e50e10f4c78574d5e87231a0096c7d9cb3439dd4b5172057a050e02946b4cf8b2bdf239ffd50a85d06
        HG_COMMON_SIZE=1847796
        HG_COMMON_FILENAME=mercurial-common_3.9.1_all.deb
    elif [ "${DISTRIB_ID}" = "Ubuntu" -a "${DISTRIB_RELEASE}" = "12.04" ]; then
        HG_DEB=1
        HG_DIGEST=f816a6ca91129c0723527d98a2978c253a3f941f4358f9f8abd6f3ab8e8601ed3efc347828aac8f0d0762f819f9b777299e31037c39eb0c5af05aa76ac25c3bf
        HG_SIZE=55144
        HG_FILENAME=mercurial_3.9.1_amd64.deb

        HG_COMMON_DIGEST=ac2b2fab9f19438c77147dca8df5020d10b129052e6c5f77ebe9a4c21eb0cedb1acfe25b146577bf5e9b66f3d6cfca2474f7575adfba1b3849b66bf5bc321015
        HG_COMMON_SIZE=2993590
        HG_COMMON_FILENAME=mercurial-common_3.9.4_all.deb
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
mv robustcheckout.py /usr/local/mercurial/robustcheckout.py
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
