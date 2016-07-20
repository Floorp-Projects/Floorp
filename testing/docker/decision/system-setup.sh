#!/usr/bin/env bash

set -v -e

test `whoami` == 'root'

apt-get update
apt-get install -y --force-yes --no-install-recommends \
    ca-certificates \
    curl \
    jq \
    python

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

# Install node
tooltool_fetch <<'EOF'
[
{
    "size": 5676610,
    "digest": "ce27b788dfd141a5ba7674332825fc136fe2c4f49a319dd19b3a87c8fffa7a97d86cbb8535661c9a68c9122719aa969fc6a8c886458a0df9fc822eec99ed130b",
    "algorithm": "sha512",
    "filename": "node-v0.10.36-linux-x64.tar.gz"
}
]

EOF
tar -C /usr/local -xz --strip-components 1 < node-*.tar.gz
node -v  # verify

npm install -g taskcluster-vcs@2.3.34

cd /
rm -rf $BUILD
apt-get clean
apt-get autoclean
rm $0
