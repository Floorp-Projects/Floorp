#!/bin/sh

cd /builds/worker
mkdir -p artifacts
python3 "$GECKO_PATH/taskcluster/scripts/misc/are-we-esmified-yet.py" > artifacts/are-we-esmified-yet.json
