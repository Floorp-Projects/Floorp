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
        HG_DIGEST=cbc3eafbc7598c7eafee81f4fb95f8d58dea5fede0fca6a04334eaa29667b9b464e3070fa24be91276d7294ba4629d7b7a648cbf969256289e9a28f5da684d09
        HG_SIZE=250774
        HG_FILENAME=mercurial_4.7.1_amd64.deb

        HG_COMMON_DIGEST=15f9c72dba116d33c2d60831bc17cd714d66b830089aebe547c846b910dbc929200f7863e167a8dade67c77c4347b8e967e6da505c2fdffa4faaa7143eccdfd8
        HG_COMMON_SIZE=2315652
        HG_COMMON_FILENAME=mercurial-common_4.7.1_all.deb
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
    "size": 6480135,
    "digest": "04d3f97dd4a0f36c6f6d639d8eccc7e4f29b2dc211fa69e7fc17dae0eb954f2ddaaf04f70facc6b968a166db7c07ed80792575d7a27e80bc0c1a43fc38b5e536",
    "algorithm": "sha512",
    "filename": "mercurial-4.7.1.tar.gz"
  }
]
EOF

   ${PIP_PATH} install mercurial-4.7.1.tar.gz
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
