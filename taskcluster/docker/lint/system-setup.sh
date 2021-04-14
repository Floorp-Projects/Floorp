#!/usr/bin/env bash
# This allows packages to be installed without human interaction
export DEBIAN_FRONTEND=noninteractive

set -ve

test "$(whoami)" == 'root'

mkdir -p /setup
cd /setup

apt_packages=()
apt_packages+=('curl')
apt_packages+=('iproute2')
apt_packages+=('locales')
apt_packages+=('git')
apt_packages+=('graphviz')
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
# zstandard
###
pip3 install -r /build/zstandard_requirements.txt

###
# ESLint Setup
###

# install node
# shellcheck disable=SC1091
. install-node.sh

npm install -g yarn@1.9.4

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

###
# rustfmt and clippy
###

cd /setup
export RUSTUP_HOME=/build/rust
export CARGO_HOME="$RUSTUP_HOME"
mkdir -p "$CARGO_HOME"
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
"$RUSTUP_HOME"/bin/rustup component add rustfmt
"$RUSTUP_HOME"/bin/rustup component add clippy
"$RUSTUP_HOME"/bin/rustc --version
"$RUSTUP_HOME"/bin/rustfmt --version
"$CARGO_HOME"/bin/cargo clippy --version

cd /
rm -rf /setup
