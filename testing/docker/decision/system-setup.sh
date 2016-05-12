#!/usr/bin/env bash

set -v -e

test `whoami` == 'root'

apt-get update
apt-get install -y --force-yes --no-install-recommends \
    autotools-dev \
    blt-dev \
    bzip2 \
    curl \
    ca-certificates \
    dpkg-dev \
    gcc-multilib \
    g++-multilib \
    jq \
    libbluetooth-dev \
    libbz2-dev \
    libexpat1-dev \
    libffi-dev \
    libffi6 \
    libffi6-dbg \
    libgdbm-dev \
    libgpm2 \
    libncursesw5-dev \
    libreadline-dev \
    libsqlite3-dev \
    libssl-dev \
    libtinfo-dev \
    make \
    mime-support \
    netbase \
    net-tools \
    python-dev \
    python-pip \
    python-crypto \
    python-mox3 \
    python-pil \
    python-ply \
    quilt \
    tar \
    tk-dev \
    xz-utils \
    zlib1g-dev

BUILD=/root/build
mkdir $BUILD

tooltool_fetch() {
    cat >manifest.tt
    python $BUILD/tooltool.py fetch
    rm manifest.tt
}

curl https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py > ${BUILD}/tooltool.py

cd $BUILD
tooltool_fetch <<'EOF'
[
{
    "size": 12250696,
    "digest": "67615a6defbcda062f15a09f9dd3b9441afd01a8cc3255e5bc45b925378a0ddc38d468b7701176f6cc153ec52a4f21671b433780d9bde343aa9b9c1b2ae29feb",
    "algorithm": "sha512",
    "filename": "Python-2.7.10.tar.xz",
    "unpack": true
}
]
EOF

cd Python-2.7.10
./configure --prefix /usr/local/lib/python2.7.10
make -j$(nproc)
make install

PATH=/usr/local/lib/python2.7.10/bin/:$PATH
python --version

# Enough python utilities to get "peep" working
cd $BUILD
tooltool_fetch <<'EOF'
[
{
    "size": 630700,
    "digest": "1367f3a10c1fef2f8061e430585f1927f6bd7c416e764d65cea1f4255824d549efa77beef8ff784bbd62c307b4b1123502e7b3fd01a243c0cc5b433a841cc8b5",
    "algorithm": "sha512",
    "filename": "setuptools-18.1.tar.gz",
    "unpack": true
},
{
    "size": 1051205,
    "digest": "e7d2e003ec60fce5a75a6a23711d7f9b155e898faebcf55f3abdd912ef513f4e0cf43daca8f9da7179a7a4efe6e4a625a532d051349818847df1364eb5b326de",
    "algorithm": "sha512",
    "filename": "pip-6.1.1.tar.gz",
    "unpack": true
},
{
    "size": 26912,
    "digest": "9d730ed7852d4d217aaddda959cd5f871ef1b26dd6c513a3780bbb04a5a93a49d6b78e95c2274451a1311c10cc0a72755b269dc9af62640474e6e73a1abec370",
    "algorithm": "sha512",
    "filename": "peep-2.4.1.tar.gz",
    "unpack": false
}
]
EOF

cd $BUILD
cd setuptools-18.1
python setup.py install
# NOTE: latest peep is not compatible with pip>=7.0
# https://github.com/erikrose/peep/pull/94

cd $BUILD
cd pip-6.1.1
python setup.py install

cd $BUILD
pip install peep-2.4.1.tar.gz

# Peep (latest)
cd $BUILD
pip install peep

# remaining Python utilities are installed with `peep` from upstream
# repositories; peep verifies file integrity for us
cat >requirements.txt <<'EOF'
# sha256: 90pZQ6kAXB6Je8-H9-ivfgDAb6l3e5rWkfafn6VKh9g
virtualenv==13.1.2

# sha256: wJnELXTi1SC2HdNyzZlrD6dgXAZheDT9exPHm5qaWzA
mercurial==3.7.3
EOF
peep install -r requirements.txt

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
