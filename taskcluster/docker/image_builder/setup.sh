#!/bin/bash -vex
set -v -e -x

export DEBIAN_FRONTEND=noninteractive

# Update apt-get lists
apt-get update -y

# Install dependencies
apt-get install -y --no-install-recommends \
    curl \
    tar \
    jq \
    python \
    python-requests \
    python-requests-unixsocket

# Extra dependencies only needed for image building. Will be removed at
# end of script.
apt-get install -y python-pip

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

# Install python-zstandard.
(
cd /setup
tooltool_fetch <<EOF
[
  {
    "size": 463794,
    "visibility": "public",
    "digest": "c6ba906403e5c18b374faf9f676b10f0988b9f4067bd6c52c548d7dee58fac79974babfd5c438aef8da0a5260158116db69b11f2a52a775772d9904b9d86fdbc",
    "algorithm": "sha512",
    "filename": "zstandard-0.8.0.tar.gz"
  }
]
EOF
)

/usr/bin/pip -v install /setup/zstandard-0.8.0.tar.gz

# python-pip only needed to install python-zstandard. Removing it removes
# several hundred MB of dependencies from the image.
apt-get purge -y python-pip

# Purge apt-get caches to minimize image size
apt-get auto-remove -y
apt-get clean -y
rm -rf /var/lib/apt/lists/

# Remove this script
rm -rf /setup/
