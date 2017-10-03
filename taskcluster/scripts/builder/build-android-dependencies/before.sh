#!/bin/bash -vex

set -x -e

echo "running as" $(id)

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

set -v

mkdir -p ${NEXUS_WORK}/conf
cp /builds/worker/workspace/build/src/taskcluster/scripts/builder/build-android-dependencies/nexus.xml ${NEXUS_WORK}/conf/nexus.xml

RUN_AS_USER=worker /opt/sonatype/nexus/bin/nexus restart

# Wait "a while" for Nexus to actually start.  Don't fail if this fails.
wget --quiet --retry-connrefused --waitretry=2 --tries=100 \
  http://localhost:8081/nexus/service/local/status || true
rm -rf status

# It's helpful when debugging to see the "latest state".
curl http://localhost:8081/nexus/service/local/status || true

# Verify Nexus has actually started.  Fail if this fails.
curl --fail --silent --location http://localhost:8081/nexus/service/local/status | grep '<state>STARTED</state>'

# It's helpful when debugging to see the repository configurations.
curl http://localhost:8081/nexus/service/local/repositories || true
