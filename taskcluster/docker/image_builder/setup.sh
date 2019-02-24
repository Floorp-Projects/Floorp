#!/bin/bash -vex
set -v -e -x

export DEBIAN_FRONTEND=noninteractive

# Update apt-get lists
apt-get update -y

# Install dependencies
apt-get install -y --no-install-recommends \
    socat \
    python-requests \
    python-requests-unixsocket \
    python3.5 \
    python3-minimal \
    python3-requests \
    python3-requests-unixsocket

# Extra dependencies only needed for image building. Will be removed at
# end of script.
apt-get install -y python-pip python3-pip

# Install mercurial
# shellcheck disable=SC1091
. /setup/common.sh
# shellcheck disable=SC1091
. /setup/install-mercurial.sh

# Install build-image.sh script
chmod +x /usr/local/bin/build-image.sh
chmod +x /usr/local/bin/run-task
chmod +x /usr/local/bin/download-and-compress

# Create workspace
mkdir -p /builds/worker/workspace

# We need to install for both Python 2 and 3 because `mach taskcluster-load-image`
# uses Python 2 and `download-and-compress` uses Python 3.
# We also need to make sure to explicitly install python3-distutils so that it doesn't get purged later
apt-get install -y python3-distutils
/usr/bin/pip -v install -r /setup/requirements-py2.txt
/usr/bin/pip3 -v install -r /setup/requirements-py3.txt

# python-pip only needed to install python-zstandard. Removing it removes
# several hundred MB of dependencies from the image.
apt-get purge -y python-pip python3-pip

# Purge apt-get caches to minimize image size
apt-get auto-remove -y
apt-get clean -y
rm -rf /var/lib/apt/lists/

# Remove this script
rm -rf /setup/
