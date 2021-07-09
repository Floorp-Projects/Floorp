#!/usr/bin/env bash

set -v -e

test "$(whoami)" == 'root'

apt-get update
apt-get install \
    python \
    sudo \
    python3-yaml

rm -rf /var/lib/apt/lists/
rm "$0"
