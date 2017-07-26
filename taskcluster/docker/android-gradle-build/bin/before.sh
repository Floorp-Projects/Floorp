#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/workspace}
: GRADLE_VERSION ${GRADLE_VERSION:=2.7}

set -v

# Populate $WORKSPACE/java/urs/lib/jvm/java_home.  $topsrcdir hasn't
# been checked out yet, so we can't put this directly into
# $topsrcdir/java_home.
. $HOME/bin/repackage-jdk-centos.sh

export JAVA_HOME=$WORKSPACE/java/usr/lib/jvm/java_home
export PATH=$PATH:$JAVA_HOME/bin

# Frowned upon, but simplest.
RUN_AS_USER=root NEXUS_WORK=${WORKSPACE}/nexus /opt/sonatype/nexus/bin/nexus restart

# Wait "a while" for Nexus to actually start.  Don't fail if this fails.
wget --quiet --retry-connrefused --waitretry=2 --tries=100 \
  http://localhost:8081/nexus/service/local/status || true
rm -rf status

# Verify Nexus has actually started.  Fail if this fails.
curl --fail --silent --location http://localhost:8081/nexus/service/local/status | grep '<state>STARTED</state>'
