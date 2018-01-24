#!/usr/bin/env bash
# This allows ubuntu-desktop to be installed without human interaction
export DEBIAN_FRONTEND=noninteractive

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('locales')
apt_packages+=('git')
apt_packages+=('python')
apt_packages+=('python-pip')
apt_packages+=('python3')
apt_packages+=('python3-pip')
apt_packages+=('shellcheck')
apt_packages+=('sudo')
apt_packages+=('wget')
apt_packages+=('xz-utils')

apt-get update
apt-get install -y "${apt_packages[@]}"

# Without this we get spurious "LC_ALL: cannot change locale (en_US.UTF-8)" errors,
# and python scripts raise UnicodeEncodeError when trying to print unicode characters.
locale-gen en_US.UTF-8
dpkg-reconfigure locales

su -c 'git config --global user.email "worker@mozilla.test"' worker
su -c 'git config --global user.name "worker"' worker

tooltool_fetch() {
    cat >manifest.tt
    /build/tooltool.py fetch
    rm manifest.tt
}

cd /build
# shellcheck disable=SC1091
. install-mercurial.sh

###
# ESLint Setup
###

# install node
# shellcheck disable=SC1091
. install-node.sh

###
# jsdoc Setup
###

npm install -g jsdoc@3.5.5

/build/tooltool.py fetch -m /tmp/eslint.tt
mv /build/node_modules /build/node_modules_eslint
/build/tooltool.py fetch -m /tmp/eslint-plugin-mozilla.tt
mv /build/node_modules /build/node_modules_eslint-plugin-mozilla

###
# fzf setup
###

tooltool_fetch <<EOF
[
  {
    "size": 866160,
    "digest": "9f0ef6bf44b8622bd0e4e8b0b5b5c714c0a2ce4487e6f234e7d4caac458164c521949f4d84b8296274e8bd20966f835e26f6492ba499405d38b620181e82429e",
    "algorithm": "sha512",
    "filename": "fzf-0.16.11-linux_amd64.tgz",
    "unpack": true
  }
]
EOF
mv fzf /usr/local/bin

###
# Flake8 Setup
###

cd /setup

pip install --require-hashes -r /tmp/flake8_requirements.txt

###
# tox Setup
###

cd /setup

pip install --require-hashes -r /tmp/tox_requirements.txt

cd /
rm -rf /setup
