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
    lib64z1-dev

rm /tmp/system-setup.sh
