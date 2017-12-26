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
        HG_DIGEST=458746bd82b4732c72c611f1041f77a47a683bc75ff3f6ab7ed86ea394f48d94cd7e2d3d1d5b020906318a9a24bea27401a3a63d7e645514dbc2cb581621977f
        HG_SIZE=193710
        HG_FILENAME=mercurial_4.4.2_amd64.deb

        HG_COMMON_DIGEST=8074efbfff974f0bbdd0c3be3d272cc7a634456921e04db31369fbec1c9256ddaf44bdbe120f6f33113d2be9324a1537048028ebaaf205c6659e476a757358fd
        HG_COMMON_SIZE=2097892
        HG_COMMON_FILENAME=mercurial-common_4.4.2_all.deb
    elif [ "${DISTRIB_ID}" = "Ubuntu" -a "${DISTRIB_RELEASE}" = "12.04" ]; then
        echo "Ubuntu 12.04 not supported"
        exit 1
    fi

    CERT_PATH=/etc/ssl/certs/ca-certificates.crt

elif [ -f /etc/os-release ]; then
    . /etc/os-release

    if [ "${ID}" = "debian" ]; then
        if [ -f /usr/bin/pip2 ]; then
            PIP_PATH=/usr/bin/pip2
        elif [ -f /usr/bin/pip ]; then
            # Versions of debian that don't have pip2 have pip pointing to the python2 version.
            PIP_PATH=/usr/bin/pip
        else
            echo "We currently require Python 2.7 and pip to run Mercurial"
            exit 1
        fi
    else
        echo "Unsupported debian-like system with ID '${ID}' and VERSION_ID '${VERSION_ID}'"
        exit 1
    fi

    CERT_PATH=/etc/ssl/certs/ca-certificates.crt

elif [ -f /etc/centos-release ]; then
    CENTOS_VERSION=`rpm -q --queryformat '%{VERSION}' centos-release`
    if [ "${CENTOS_VERSION}" = "6" ]; then
        if [ -f /usr/bin/pip2.7 ]; then
            PIP_PATH=/usr/bin/pip2.7
        else
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
    "size": 5647013,
    "digest": "3d1d103689eac4f50cc1005be44144b37d75ebfac3ff3b4fc90d6f41fbee46e107a168d04f2c366ce7cca2733ea4e5b5127df462af8e253f61a72f8938833993",
    "algorithm": "sha512",
    "filename": "mercurial-4.4.2.tar.gz"
  }
]
EOF

   ${PIP_PATH} install mercurial-4.4.2.tar.gz
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
share =
sparse =
robustcheckout = /usr/local/mercurial/robustcheckout.py

[hostsecurity]
# When running a modern Python, Mercurial will default to TLS 1.1+.
# When running on a legacy Python, Mercurial will default to TLS 1.0+.
# There is no good reason we shouldn't be running a modern Python
# capable of speaking TLS 1.2. And the only Mercurial servers we care
# about should be running TLS 1.2. So make TLS 1.2 the minimum.
minimumprotocol = tls1.2

# Settings to make 1-click loaners more useful.
[extensions]
histedit =
rebase =

[diff]
git = 1
showfunc = 1

[pager]
pager = LESS=FRSXQ less
EOF

chmod 644 /etc/mercurial/hgrc
