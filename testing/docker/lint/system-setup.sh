#!/usr/bin/env bash
# This allows ubuntu-desktop to be installed without human interaction
export DEBIAN_FRONTEND=noninteractive

set -ve

test `whoami` == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('python')
apt_packages+=('python-pip')
apt_packages+=('sudo')
apt_packages+=('xz-utils')

apt-get update
apt-get install -y ${apt_packages[@]}

# Without this we get spurious "LC_ALL: cannot change locale (en_US.UTF-8)" errors,
# and python scripts raise UnicodeEncodeError when trying to print unicode characters.
locale-gen en_US.UTF-8
dpkg-reconfigure locales

tooltool_fetch() {
    cat >manifest.tt
    /build/tooltool.py fetch
    rm manifest.tt
}

cd /build
. install-mercurial.sh

###
# ESLint Setup
###

# install node

# For future reference things like this don't need to be uploaded to tooltool, as long
# as we verify the hash, we can download it from the external net.
cd /setup
tooltool_fetch <<'EOF'
[
{
    "size": 8310316,
    "digest": "95f4fa3d9b215348393dfac4a1c5eff72e9ef85dca38eb69cc8e6c1fe5aada0136c3b182dc04ed5c19fb69f0ac7df85d9c4045b9eb382fcb545b0ccacfece25b",
    "algorithm": "sha512",
    "filename": "node-v4.4.5-linux-x64.tar.xz"
}
]
EOF
tar -C /usr/local --strip-components 1 -xJ < node-*.tar.xz
node -v  # verify
npm -v

###
# Flake8 Setup
###

cd /setup
cat >requirements.txt <<'EOF'
flake8==2.5.4 \
  --hash=sha256:fb5a67af4024622287a76abf6b7fe4fb3cfacf765a790976ce64f52c44c88e4a
mccabe==0.4.0 \
  --hash=sha256:cbc2938f6c01061bc6d21d0c838c2489664755cb18676f0734d7617f4577d09e
pep8==1.7.0 \
  --hash=sha256:4fc2e478addcf17016657dff30b2d8d611e8341fac19ccf2768802f6635d7b8a
pyflakes==1.2.3 \
  --hash=sha256:e87bac26c62ea5b45067cc89e4a12f56e1483f1f2cda17e7c9b375b9fd2f40da
EOF

pip install --require-hashes -r requirements.txt

cd /
rm -rf /setup

