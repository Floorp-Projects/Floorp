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
            #HG_DIGEST=68f020e5584d58855c46b5e36e7cd7d480a69effcdc927dcb1f97cb9b638d23e058ed113dbfd817a047ba0550d287cedcbec0cee67a9ef2519657339fe2f9426
            #HG_SIZE=4175628
            #HG_FILENAME=mercurial-3.9.1-1.x86_64.rpm
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
"size": 4797967,
"visibility": "public",
"digest": "d96e45cafd36be692d6ce5259e18140641c24f73d4731ff767df0f39af425b0630c687436fc0f53d5882495ceacacaadd5e19f8f7c701b4b94c48631123b4666",
"algorithm": "sha512",
"filename": "mercurial-3.9.1.tar.gz"
}
]
EOF

   ${PIP_PATH} install mercurial-3.9.1.tar.gz
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
