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
    python-virtualenv \
    valgrind \
    uuid-dev \
    sqlite3

# the Android SDK contains some 32-bit binaries (aapt among them) that require this
apt-get install -y lib32z1

rm /tmp/system-setup.sh
