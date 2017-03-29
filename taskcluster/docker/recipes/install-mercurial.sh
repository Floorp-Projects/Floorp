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
        HG_DIGEST=d6084b7d02d7c428165899e06ac1961deff5485ecb4c61f66993109b1c4de3a64af1fb143fd6a00c8e64c877f39250a6e36a9939ff630a2e66be14bc9a05477e
        HG_SIZE=159986
        HG_FILENAME=mercurial_4.1.1_amd64.deb

        HG_COMMON_DIGEST=1a68d78bf080c1b02108feb8ca5f0a64a62ed182612b4307ed7864a77038fbc3e5ba7d0a1ef54a3f2d7bf722b52fc298be44e36cb84d11a2a46c29753d1b05a3
        HG_COMMON_SIZE=1918874
        HG_COMMON_FILENAME=mercurial-common_4.1.1_all.deb
    elif [ "${DISTRIB_ID}" = "Ubuntu" -a "${DISTRIB_RELEASE}" = "12.04" ]; then
        HG_DEB=1
        HG_DIGEST=3c021d596fce58d263fc204c7cc6313fc7caee0b6ae074cefad0a816126df19a2430fea316c1427bb5070b6aee59b6a55d1322174e1687dab592868512500a9c
        HG_SIZE=167936
        HG_FILENAME=mercurial_4.1.1_amd64.deb

        HG_COMMON_DIGEST=87bfeeca7609639ae293e1090dac3fa930b6369e299506e68ba517a50362c4cbf87f6ed79e8cfa2fc07318a6610d1f4e7f2c381004a20d623ef5f15a95fa3556
        HG_COMMON_SIZE=3092066
        HG_COMMON_FILENAME=mercurial-common_4.1.1_all.deb
    fi

    CERT_PATH=/etc/ssl/certs/ca-certificates.crt

elif [ -f /etc/centos-release ]; then
    CENTOS_VERSION=`rpm -q --queryformat '%{VERSION}' centos-release`
    if [ "${CENTOS_VERSION}" = "6" ]; then
        if [ -f /usr/bin/pip2.7 ]; then
            PIP_PATH=/usr/bin/pip2.7
        else
            # The following RPM is "linked" against Python 2.6, which doesn't
            # support TLS 1.2. Given the security implications of an insecure
            # version control tool, we choose to prefer a Mercurial built using
            # Python 2.7 that supports TLS 1.2. Before you uncomment the code
            # below, think long and hard about the implications of limiting
            # Mercurial to TLS 1.0.
            #HG_RPM=1
            #HG_DIGEST=812eff1984087e7eb6ce3741cd0c8bacfe31e9392e94e7bb894137bf3d46e4f743e34044c6c73883189e6ea65cfe7d35788602dc63f30d83468164764c71125f
            #HG_SIZE=4437392
            #HG_FILENAME=mercurial-4.1.1.x86_64.rpm
            echo "We currently require Python 2.7 and /usr/bin/pip2.7 to run Mercurial"
            exit 1
        fi
    else
        echo "Unsupported CentOS version: ${CENTOS_VERSION}"
        exit 1
    fi

    CERT_PATH=/etc/ssl/certs/ca-bundle.crt
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
elif [ -n "${HG_RPM}" ]; then
tooltool_fetch <<EOF
[
  {
    "size": ${HG_SIZE},
    "digest": "${HG_DIGEST}",
    "algorithm": "sha512",
    "filename": "${HG_FILENAME}"
  }
]
EOF

    rpm -i ${HG_FILENAME}
elif [ -n "${PIP_PATH}" ]; then
tooltool_fetch <<EOF
[
  {
    "size": 6838504,
    "visibility": "public",
    "digest": "af1450d42308e24538d1aea82c25676d9de74eda89b9b87ee69dc39241551dbe6343c8b41a30f35c476bdad012f18127f67264d354bf48ed41c93f47d48575a7",
    "algorithm": "sha512",
    "filename": "mercurial-4.1.1.tar.gz"
  }
]
EOF

   ${PIP_PATH} install mercurial-4.1.1.tar.gz
else
    echo "Do not know how to install Mercurial on this OS"
    exit 1
fi

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
cacerts = ${CERT_PATH}

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
