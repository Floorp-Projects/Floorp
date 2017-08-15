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
        HG_DIGEST=dd4dd7759fe73985b6a0424b34a3036d130c26defdd866a9fdd7302e40c7417433b93f020497ceb40593eaead8e86be55e48340887015645202b47ff7b0d7ac6
        HG_SIZE=181722
        HG_FILENAME=mercurial_4.3.1_amd64.deb

        HG_COMMON_DIGEST=045f7e07f1e2e0fef767b2f50a7e9ab37d5da0bfead5ddf473ae044b61a4566aed2d6f2706f52d227947d713ef8e89eb9a269288f08e52924e4de88a39cd7ac0
        HG_COMMON_SIZE=2017628
        HG_COMMON_FILENAME=mercurial-common_4.3.1_all.deb
    elif [ "${DISTRIB_ID}" = "Ubuntu" -a "${DISTRIB_RELEASE}" = "12.04" ]; then
        HG_DEB=1
        HG_DIGEST=67823aa455c59dbdc24ec1f044b0afdb5c03520ef3601509cb5466dc0ac332846caf96176f07de501c568236f6909e55dfc8f4b02f8c69fa593a4abca9abfeb8
        HG_SIZE=167880
        HG_FILENAME=mercurial_4.1.2_amd64.deb

        HG_COMMON_DIGEST=5e1c462a9b699d2068f7a0c14589f347ca719c216181ef7a625033df757185eeb3a8fed57986829a7943f16af5a8d66ddf457cc7fc4af557be88eb09486fe665
        HG_COMMON_SIZE=3091596
        HG_COMMON_FILENAME=mercurial-common_4.1.2_all.deb
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
            #HG_DIGEST=c64e00c74402cd9c4ef9792177354fa6ff9c8103f41358f0eab2b15dba900d47d04ea582c6c6ebb80cf52495a28433987ffb67a5f39cd843b6638e3fa46921c8
            #HG_SIZE=4437360
            #HG_FILENAME=mercurial-4.1.2.x86_64.rpm
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
    "size": 5475042,
    "digest": "4c42d06b7f111a3e825dd927704a30f88f0b2225cf87ab8954bf53a7fbc0edf561374dd49b13d9c10140d98ff5853a64acb5a744349727abae81d32da401922b",
    "algorithm": "sha512",
    "filename": "mercurial-4.3.1.tar.gz"
  }
]
EOF

   ${PIP_PATH} install mercurial-4.3.1.tar.gz
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
