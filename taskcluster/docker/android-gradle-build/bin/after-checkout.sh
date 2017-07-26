#!/bin/bash -vex

set -x -e

: WORKSPACE ${WORKSPACE:=/workspace}

set -v

# Populate /home/worker/workspace/build/src/java_home.
cp -R /workspace/java/usr/lib/jvm/java_home /home/worker/workspace/build/src

export JAVA_HOME=/home/worker/workspace/build/src/java_home
export PATH=$PATH:$JAVA_HOME/bin

# Populate /home/worker/.mozbuild/android-sdk-linux.
python2.7 /home/worker/workspace/build/src/python/mozboot/mozboot/android.py --artifact-mode --no-interactive
