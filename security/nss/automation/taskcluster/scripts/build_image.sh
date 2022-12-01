#!/bin/bash -vex

set -x -e -v

# Prefix errors with taskcluster error prefix so that they are parsed by Treeherder
raise_error() {
   echo
   echo "[taskcluster-image-build:error] $1"
   exit 1
}

# Ensure that the PROJECT is specified so the image can be indexed
test -n "$PROJECT" || raise_error "Project must be provided."
test -n "$HASH" || raise_error "Context Hash must be provided."

CONTEXT_PATH="/home/worker/nss/$CONTEXT_PATH"

test -d "$CONTEXT_PATH" || raise_error "Context Path $CONTEXT_PATH does not exist."
test -f "$CONTEXT_PATH/Dockerfile" || raise_error "Dockerfile must be present in $CONTEXT_PATH."

apt-get update
apt-get -y install zstd

docker build -t "$PROJECT:$HASH" "$CONTEXT_PATH"

mkdir /artifacts
docker save "$PROJECT:$HASH" | zstd > /artifacts/image.tar.zst
