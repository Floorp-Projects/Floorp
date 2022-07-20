#!/usr/bin/env bash

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('iproute2')
apt_packages+=('locales')
apt_packages+=('graphviz')
apt_packages+=('python3-pip')
apt_packages+=('python-is-python3')
apt_packages+=('shellcheck')
apt_packages+=('sudo')
apt_packages+=('wget')

apt-get update
apt-get install "${apt_packages[@]}"

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

###
# ESLint Setup
###

# install node
# shellcheck disable=SC1091
. install-node.sh

npm install -g yarn@1.22.18

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
    "size": 1161860,
    "digest": "3246470715e1ddf4c7e5136fdddd2ca269928c2de3074a98233faef189efd88fc9b28ddbe68642a31cf647a97f630941d764187006c5115e6f357d49322ef58d",
    "algorithm": "sha512",
    "filename": "fzf-0.20.0-linux_amd64.tgz",
    "unpack": true
  }
]
EOF
mv fzf /usr/local/bin

###
# codespell Setup
###

cd /setup

pip3 install --require-hashes -r /tmp/codespell_requirements.txt

###
# tox Setup
###

cd /setup

pip3 install --require-hashes -r /tmp/tox_requirements.txt

cd /
rm -rf /setup
