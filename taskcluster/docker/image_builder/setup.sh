#!/bin/bash -vex
set -v -e -x

export DEBIAN_FRONTEND=noninteractive

# Update apt-get lists
apt-get update -y

# Install dependencies
apt-get install -y --no-install-recommends \
    socat \
    python3.5 \
    python3-minimal \
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
    "size": 558068,
    "visibility": "public",
    "digest": "72b1fc542e5af36fc660d7b8d3882f0a25644d3b66316293717aabf9ba8cf578e49e2cf45e63e962c5535ec1f8b3e83248c379d34b0cab2ef1a950205ad153ce",
    "algorithm": "sha512",
    "filename": "zstandard-0.9.0.tar.gz"
  }
]
EOF
)

/usr/bin/pip -v install /setup/zstandard-0.9.0.tar.gz

# python-pip only needed to install python-zstandard. Removing it removes
# several hundred MB of dependencies from the image.
apt-get purge -y python-pip

# Purge apt-get caches to minimize image size
apt-get auto-remove -y
apt-get clean -y
rm -rf /var/lib/apt/lists/

# Remove this script
rm -rf /setup/
