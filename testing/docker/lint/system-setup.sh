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

pip install --require-hashes -r /tmp/flake8_requirements.txt

cd /
rm -rf /setup
