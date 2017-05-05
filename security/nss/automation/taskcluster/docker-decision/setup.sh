#!/usr/bin/env bash

set -v -e -x

# Update packages.
export DEBIAN_FRONTEND=noninteractive
apt-get -y update && apt-get -y upgrade

# Need those to install newer packages below.
apt-get install -y --no-install-recommends apt-utils curl ca-certificates locales

# Latest Mercurial.
apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 41BD8711B1F0EC2B0D85B91CF59CE3A8323293EE
echo "deb http://ppa.launchpad.net/mercurial-ppa/releases/ubuntu xenial main" > /etc/apt/sources.list.d/mercurial.list

# Install packages.
apt-get -y update && apt-get install -y --no-install-recommends mercurial

# Latest Node.JS.
curl -sL https://deb.nodesource.com/setup_6.x | bash -
apt-get install -y --no-install-recommends nodejs

locale-gen en_US.UTF-8
dpkg-reconfigure locales

# Cleanup.
rm -rf ~/.ccache ~/.cache
apt-get autoremove -y
apt-get clean
apt-get autoclean
rm $0
