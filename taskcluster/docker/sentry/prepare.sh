#!/bin/sh

set -o errexit
set -o nounset
set -o pipefail
set -o xtrace

apt-get -y update

# Install:
# * curl to fetch sentry-cli
# * jq to parse hgmo pushlog
# * pip so we can install yq
apt-get install -y \
    curl \
    jq \
    python3-pip

# Install yq to parse secrets YAML
pip3 install yq

# Install sentry-cli to publish releases
curl -L https://github.com/getsentry/sentry-cli/releases/download/1.63.1/sentry-cli-Linux-x86_64 -o /usr/bin/sentry-cli
chmod +x /usr/bin/sentry-cli
