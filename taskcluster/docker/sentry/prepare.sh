#!/bin/sh

set -o errexit
set -o nounset
set -o xtrace

apt-get -y update

# Install:
# * curl to fetch sentry-cli
# * jq to parse hgmo pushlog
apt-get install -y \
    curl \
    jq

# Install sentry-cli to publish releases
curl -L https://github.com/getsentry/sentry-cli/releases/download/1.63.1/sentry-cli-Linux-x86_64 -o /usr/bin/sentry-cli
chmod +x /usr/bin/sentry-cli
