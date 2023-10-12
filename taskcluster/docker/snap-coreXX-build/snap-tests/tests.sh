#!/bin/bash

set -ex

pwd

ARTIFACT_DIR=$TASKCLUSTER_ROOT_DIR/builds/worker/artifacts/
mkdir -p "$ARTIFACT_DIR"

sudo snap refresh

sudo snap refresh --hold=24h firefox

sudo snap install --name firefox --dangerous ./firefox.snap

RUNTIME_VERSION=$(snap run firefox --version | awk '{ print $3 }')

python3 -m pip install --user -r requirements.txt

sed -e "s/#RUNTIME_VERSION#/${RUNTIME_VERSION}/#" < expectations.json.in > expectations.json

python3 basic_tests.py expectations.json

cp ./*.png "$ARTIFACT_DIR/"
