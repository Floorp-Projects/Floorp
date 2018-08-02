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
        HG_DIGEST=21a5ca8170bdb05527b04cbc93f00ecef39680a2c7b80aa625f9add8b7dba0a7bff0ad58e0ba40b1956ae310f681d04302d033e398eca0e001dce10d74d0dbdc
        HG_SIZE=250748
        HG_FILENAME=mercurial_4.7_amd64.deb

        HG_COMMON_DIGEST=521e0c150b142d0bbb69fa100b96bac5bee7a108605eea1c484e8544540dcf764efffab2e3a9ad640f56674ad85c1b47647e3d504c96f7567d7d7c21299c2c25
        HG_COMMON_SIZE=2315590
        HG_COMMON_FILENAME=mercurial-common_4.7_all.deb
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
    "size": 6476268,
    "digest": "a08dfc4e296b5d162097769ab38ab85b7c5de16710bce0b6dce2a39f56cb517455c0ed634f689d07e9bd082fb7641501b7da51963844aee7ab28233cf721dec8",
    "algorithm": "sha512",
    "filename": "mercurial-4.7.tar.gz"
  }
]
EOF

   ${PIP_PATH} install mercurial-4.7.tar.gz
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
