#!/bin/bash -vex

# Set bash options to exit immediately if a pipeline exists non-zero, expand
# print a trace of commands, and make output verbose (print shell input as it's
# read)
# See https://www.gnu.org/software/bash/manual/html_node/The-Set-Builtin.html
set -x -e -v

# Prefix errors with taskcluster error prefix so that they are parsed by Treeherder
raise_error() {
  echo
  echo "[taskcluster-image-build:error] $1"
  exit 1
}

# Ensure that the PROJECT is specified so the image can be indexed
test -n "$PROJECT"    || raise_error "PROJECT must be provided."
test -n "$HASH"       || raise_error "Context HASH must be provided."
test -n "$IMAGE_NAME" || raise_error "IMAGE_NAME must be provided."

# Create artifact folder
mkdir -p /home/worker/workspace/artifacts

# Construct a CONTEXT_FILE
CONTEXT_FILE=/home/worker/workspace/context.tar

# Run ./mach taskcluster-build-image with --context-only to build context
run-task \
  --chown-recursive "/home/worker/workspace" \
  --vcs-checkout "/home/worker/checkouts/gecko" \
  -- \
  /home/worker/checkouts/gecko/mach taskcluster-build-image \
  --context-only "$CONTEXT_FILE" \
  "$IMAGE_NAME"
test -f "$CONTEXT_FILE" || raise_error "Context file wasn't created"

# Post context tar-ball to docker daemon
# This interacts directly with the docker remote API, see:
# https://docs.docker.com/engine/reference/api/docker_remote_api_v1.18/
curl -s --fail \
  -X POST \
  --header 'Content-Type: application/tar' \
  --data-binary "@$CONTEXT_FILE" \
  --unix-socket /var/run/docker.sock "http:/build?t=$IMAGE_NAME:$HASH" \
  | tee /tmp/docker-build.log \
  | jq -jr '(.status + .progress, .error | select(. != null) + "\n"), .stream | select(. != null)'

# Exit non-zero if there is error entries in the log
if cat /tmp/docker-build.log | jq -se 'add | .error' > /dev/null; then
  raise_error "Image build failed: `cat /tmp/docker-build.log | jq -rse 'add | .error'`";
fi

# Get image from docker daemon (try up to 10 times)
# This interacts directly with the docker remote API, see:
# https://docs.docker.com/engine/reference/api/docker_remote_api_v1.18/
count=0
while ! curl -s --fail -X GET  \
             --unix-socket /var/run/docker.sock "http:/images/$IMAGE_NAME:$HASH/get" \
             | zstd -3 -c -o /home/worker/workspace/artifacts/image.tar.zst; do
  ((c++)) && ((c==10)) && echo 'Failed to get image from docker' && exit 1;
  echo 'Waiting for image to be ready';
  sleep 5;
done
