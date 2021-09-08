#!/bin/sh

set -x

# Debian >= 10 doesn't have openjdk-8, so add the Debian 9 repository, which contains it.
version=$(awk -F. '{print $1}' /etc/debian_version)
if test "$version" -ge 10; then
	sed -n "s/ [^/ ]* main/ stretch main/p;q" /etc/apt/sources.list | tee /etc/apt/sources.list.d/stretch.list
	apt-get update
fi
