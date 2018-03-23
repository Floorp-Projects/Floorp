#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs and configures Mercurial.

set -e

# Detect OS.
if [ -f /etc/lsb-release ]; then
    # Disabled so linting works on Mac
    # shellcheck disable=SC1091
    . /etc/lsb-release

    if [ "${DISTRIB_ID}" = "Ubuntu" ] && [[ "${DISTRIB_RELEASE}" = "16.04" || "${DISTRIB_RELEASE}" = "17.10" || "${DISTRIB_RELEASE}" = "18.04" ]]
    then
        HG_DEB=1
        HG_DIGEST=e58ecb78fb6856161f8af1b7a50a024f1d6aa6efe50e18666aae0368ee1a44eead2f0c52e5088304b4c0e89dd74a058128e9bd184efab0142275a228aa8e0f45
        HG_SIZE=193382
        HG_FILENAME=mercurial_4.5.2_amd64.deb

        HG_COMMON_DIGEST=b69d94c91ad78a26318e3bbd2f0fda7eb3f3295755a727cde677bf143312c5dfc27ac47e800772d624ea88c6f2576fa2f585c1b8b9930ba83ddc8356661627b8
        HG_COMMON_SIZE=2141554
        HG_COMMON_FILENAME=mercurial-common_4.5.2_all.deb
    elif [ "${DISTRIB_ID}" = "Ubuntu" ] && [ "${DISTRIB_RELEASE}" = "12.04" ]
    then
        echo "Ubuntu 12.04 not supported"
        exit 1
    fi

    CERT_PATH=/etc/ssl/certs/ca-certificates.crt

elif [ -f /etc/os-release ]; then
    # Disabled so linting works on Mac
    # shellcheck disable=SC1091
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
    CENTOS_VERSION="$(rpm -q --queryformat '%{VERSION}' centos-release)"
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
    "size": 5779915,
    "digest": "f70e40cba72b7955f0ecec9c1f53ffffac26f206188617cb182e22ce4f43dc8b970ce46d12c516ef88480c3fa076a59afcddd736dffb642d8e23befaf45b4941",
    "algorithm": "sha512",
    "filename": "mercurial-4.5.2.tar.gz"
  }
]
EOF

   ${PIP_PATH} install mercurial-4.5.2.tar.gz
else
    echo "Do not know how to install Mercurial on this OS"
    exit 1
fi

chmod 644 /usr/local/mercurial/robustcheckout.py

cat >/etc/mercurial/hgrc.d/cacerts.rc <<EOF
[web]
cacerts = ${CERT_PATH}
EOF

chmod 644 /etc/mercurial/hgrc.d/cacerts.rc
