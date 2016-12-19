#!/bin/bash -vex
set -v -e -x

export DEBIAN_FRONTEND=noninteractive

# Update apt-get lists
apt-get update -y

# Install dependencies
apt-get install -y \
    curl \
    tar \
    jq \
    python \
    build-essential # Only needed for zstd installation, will be removed later

# Install mercurial
. /setup/common.sh
. /setup/install-mercurial.sh

# Install build-image.sh script
chmod +x /usr/local/bin/build-image.sh
chmod +x /usr/local/bin/run-task

# Create workspace
mkdir -p /home/worker/workspace

# Install zstd 1.1.1
cd /setup
tooltool_fetch <<EOF
[
  {
    "size": 734872,
    "visibility": "public",
    "digest": "a8817e74254f21ee5b76a21691e009ede2cdc70a78facfa453902df3e710e90e78d67f2229956d835960fd1085c33312ff273771b75f9322117d85eb35d8e695",
    "algorithm": "sha512",
    "filename": "zstd.tar.gz"
  }
]
EOF
cd -
tar -xvf /setup/zstd.tar.gz -C /setup
make -C /setup/zstd-1.1.1/programs install
rm -rf /tmp/zstd-1.1.1/ /tmp/zstd.tar.gz
apt-get purge -y build-essential

# Purge apt-get caches to minimize image size
apt-get auto-remove -y
apt-get clean -y
rm -rf /var/lib/apt/lists/

# Remove this script
rm -rf /setup/
