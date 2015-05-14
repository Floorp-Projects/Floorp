#!/usr/bin/env bash

set -ve

test `whoami` == 'root';

apt-get update -y
apt-get install -y \
    wget \
    python g++-multilib \
    git\
    nodejs-legacy \
    npm \
    curl \
    x11-utils \
    python-virtualenv

# see https://bugzilla.mozilla.org/show_bug.cgi?id=1161075
apt-get install -y openjdk-7-jdk

# the Android SDK contains some 32-bit binaries (aapt among them) that require this
apt-get install -y lib32z1

rm /tmp/system-setup.sh
