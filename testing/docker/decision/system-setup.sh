#!/usr/bin/env bash

set -v -e

test `whoami` == 'root'

apt-get update
apt-get install -y --force-yes --no-install-recommends \
    ca-certificates \
    curl \
    jq \
    python \
    sudo

BUILD=/root/build
mkdir $BUILD

tooltool_fetch() {
    cat >manifest.tt
    python2.7 /tmp/tooltool.py fetch
    rm manifest.tt
}

# Install Mercurial from custom debs since distro packages tend to lag behind.
cd $BUILD
tooltool_fetch <<EOF
[
{
    "size": 44878,
    "digest": "7b1fc1217e0dcaeea852b0af2dc559b1aafb704fbee7e29cbec75af57bacb84910a7ec92b5c33f04ee98f23b3a57f1fa451173fe7c8a96f58faefe319dc7dde1",
    "algorithm": "sha512",
    "filename": "mercurial_3.8.4_amd64.deb"
},
{
    "size": 1818422,
    "digest": "b476e2612e7495a1c7c5adfd84511aa7479e26cc9070289513ec705fbfc4c61806ce2dbcceca0e63f2e80669be416f3467a3cebb522dcb8a6aeb62cdd3df82f2",
    "algorithm": "sha512",
    "filename": "mercurial-common_3.8.4_all.deb"
}
]
EOF

dpkg -i mercurial-common_3.8.4_all.deb mercurial_3.8.4_amd64.deb

mkdir -p /usr/local/mercurial
chown 755 /usr/local/mercurial
cd /usr/local/mercurial
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

chmod 644 /usr/local/mercurial/robustcheckout.py

# Install a global hgrc file with reasonable defaults.
mkdir -p /etc/mercurial
cat >/etc/mercurial/hgrc <<EOF
# By default the progress bar starts after 3s and updates every 0.1s. We
# change this so it shows and updates every 1.0s.
[progress]
delay = 1.0
refresh = 1.0

[web]
cacerts = /etc/ssl/certs/ca-certificates.crt

[extensions]
robustcheckout = /usr/local/mercurial/robustcheckout.py
EOF

chmod 644 /etc/mercurial/hgrc

cd /
rm -rf $BUILD
apt-get clean
apt-get autoclean
rm $0
