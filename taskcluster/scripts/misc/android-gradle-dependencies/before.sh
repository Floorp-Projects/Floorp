#!/bin/bash -vex

set -x -e

echo "running as" $(id)

if [[ -z "${WORKSPACE}" ]]; then
  export WORKSPACE=/builds/worker/workspace
fi

set -v

# Download the gradle-python-envs plugin
# See https://github.com/gradle/plugin-portal-requests/issues/164
pushd ${WORKSPACE}
mkdir -p android-gradle-dependencies
pushd android-gradle-dependencies

PYTHON_ENVS_VERSION="0.0.31"

PYTHON_ENVS_BASE_URL=https://plugins.gradle.org/m2/gradle/plugin/com/jetbrains/python/gradle-python-envs

wget --no-parent --recursive --execute robots=off "${PYTHON_ENVS_BASE_URL}/${PYTHON_ENVS_VERSION}/"
popd
popd

# Export NEXUS_WORK so that `after.sh` can use it.
export NEXUS_WORK=${WORKSPACE}/sonatype-nexus-work
mkdir -p ${NEXUS_WORK}/conf
cp ${WORKSPACE}/build/src/taskcluster/scripts/misc/android-gradle-dependencies/nexus.xml ${NEXUS_WORK}/conf/nexus.xml

RUN_AS_USER=worker $MOZ_FETCHES_DIR/sonatype-nexus/bin/nexus restart

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
