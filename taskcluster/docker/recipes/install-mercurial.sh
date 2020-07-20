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
        HG_DIGEST=54a215232a340139707a968b58943c2903a8297f0da32f96622d1acab47de6013a5b96d2ca4ba241b1fee142b4098a6cdd236b308a1657c31f42807d7385d327
        HG_SIZE=278440
        HG_FILENAME=mercurial_4.8.1_amd64.deb

        HG_COMMON_DIGEST=5577fec8d0f6643d17751b3f6be76b0c2bb888ae1920a8b085245e05110e3d5cfe1c4e9d51e334ab0dd0865fe553c63c704e72852e00b71eb668980cb6b33fa4
        HG_COMMON_SIZE=2439436
        HG_COMMON_FILENAME=mercurial-common_4.8.1_all.deb
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

    dpkg -i --auto-deconfigure ${HG_COMMON_FILENAME} ${HG_FILENAME}
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
    "size": 6869733,
    "digest": "a4485c22f9bb0bb752bf42941f613cb3542c66cbec5d7d49be2090ac544f5dca0f476e4535a56e3f4f4f5fc02fb12739e6d1c7b407264fc2ba4b19b0230b9f93",
    "algorithm": "sha512",
    "filename": "mercurial-4.8.1.tar.gz"
  }
]
EOF

   ${PIP_PATH} install mercurial-4.8.1.tar.gz
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
