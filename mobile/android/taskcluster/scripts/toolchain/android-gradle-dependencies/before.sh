#!/bin/bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is a copy of
# https://searchfox.org/mozilla-central/rev/2cd2d511c0d94a34fb7fa3b746f54170ee759e35/taskcluster/scripts/misc/android-gradle-dependencies/before.sh.
# `misc` was renamed into `toolchain` and `/builds/worker/workspace/build` was changed into
# `/builds/worker/checkouts/`

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

set -v

mkdir -p ${NEXUS_WORK}/conf
cp /builds/worker/checkouts/vcs/taskcluster/scripts/toolchain/android-gradle-dependencies/nexus.xml ${NEXUS_WORK}/conf/nexus.xml

# For the Android build system we want Java 11. However this Nexus installation requires Java 8.
PATH="/usr/lib/jvm/java-8-openjdk-amd64/jre/bin/:$PATH" RUN_AS_USER=worker /opt/sonatype/nexus/bin/nexus restart

# Wait "a while" for Nexus to actually start.  Don't fail if this fails.
curl --retry-connrefused --retry-delay 2 --retry 100 \
  http://localhost:8081/nexus/service/local/status || true
rm -rf status

# It's helpful when debugging to see the "latest state".
curl http://localhost:8081/nexus/service/local/status || true

# Verify Nexus has actually started.  Fail if this fails.
curl --fail --silent --location http://localhost:8081/nexus/service/local/status | grep '<state>STARTED</state>'

# It's helpful when debugging to see the repository configurations.
curl http://localhost:8081/nexus/service/local/repositories || true
