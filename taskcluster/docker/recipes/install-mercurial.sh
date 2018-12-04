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
        HG_DIGEST=eca872b1e0007f6dee40013d41ba3e17fe636500051304b69720a14aacab298ebed2999ff2433cf462f8931ef9c1d3bbac8544374056a76348da738c186dcc20
        HG_SIZE=278406
        HG_FILENAME=mercurial_4.8_amd64.deb

        HG_COMMON_DIGEST=ba4d267aba2c3fe02e9cd9227b9a910e4e287971ff5ebd60b238e11ce97d82b2316b9d43f54d3f2384e9947a7a7f403c982329fe6ca6309bdfb8ba91ac345f48
        HG_COMMON_SIZE=2435262
        HG_COMMON_FILENAME=mercurial-common_4.8_all.deb
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
    "size": 6865809,
    "digest": "83be5119355da6e43635a8ada3c152420314e72c4a7b98717ac8b5feb628f3ce96c0ca137cb4a624d441e4f800d9ca78ada3351ca403815c06ab151cc720077d",
    "algorithm": "sha512",
    "filename": "mercurial-4.8.tar.gz"
  }
]
EOF

   ${PIP_PATH} install mercurial-4.8.tar.gz
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
