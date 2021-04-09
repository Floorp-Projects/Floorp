#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -vex

export UPDATEBOT_REVISION=4710f6bae44855dc5a3d8d1389768731fd65e792
export SQLPROXY_REVISION=fb1939ab92846761595833361c6b0b0ecd543861

export DEBIAN_FRONTEND=noninteractive

# Update apt-get lists
apt-get update -y

# Install dependencies
apt-get install -y --no-install-recommends \
    arcanist \
    bzr \
    ca-certificates \
    curl \
    golang-go \
    gcc \
    libc6-dev \
    python-requests \
    python-requests-unixsocket \
    python3.5 \
    python3-minimal \
    python3-wheel \
    python3-pip \
    python3-venv \
    python3-requests \
    python3-requests-unixsocket \
    python3-setuptools \
    openssh-client \
    wget

mkdir -p /builds/worker/.mozbuild
chown -R worker:worker /builds/worker/

export GOPATH=/builds/worker/go

# Build Google's Cloud SQL Proxy from source
cd /builds/worker/
mkdir cloud_sql_proxy
cd cloud_sql_proxy
go mod init .
go get github.com/GoogleCloudPlatform/cloudsql-proxy/cmd/cloud_sql_proxy@$SQLPROXY_REVISION

# Check out source code
cd /builds/worker/
git clone https://github.com/mozilla-services/updatebot.git
cd updatebot
git checkout $UPDATEBOT_REVISION

# Set up dependencies
cd /builds/worker/
chown -R worker:worker .
chown -R worker:worker .*

python3 -m pip install -U pip
python3 -m pip install poetry

rm -rf /setup