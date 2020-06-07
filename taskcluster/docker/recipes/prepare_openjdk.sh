#!/bin/sh

set -x

# Debian 10 doesn't have openjdk-8, so add the Debian 9 repository, which contains it.
if grep -q ^10\\. /etc/debian_version; then
	sed s/buster/stretch/ /etc/apt/sources.list | tee /etc/apt/sources.list.d/stretch.list
	apt-get update
fi
