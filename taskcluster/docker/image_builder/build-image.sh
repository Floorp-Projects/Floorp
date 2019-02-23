#!/bin/bash -vex

# Set bash options to exit immediately if a pipeline exists non-zero, expand
# print a trace of commands, and make output verbose (print shell input as it's
# read)
# See https://www.gnu.org/software/bash/manual/html_node/The-Set-Builtin.html
set -x -e -v -o pipefail

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

# The docker socket is mounted by the taskcluster worker in a way that prevents
# us changing its permissions to allow the worker user to access it. Create a
# proxy socket that the worker user can use.
export DOCKER_SOCKET=/var/run/docker.proxy
socat UNIX-LISTEN:$DOCKER_SOCKET,fork,group=worker,mode=0775 UNIX-CLIENT:/var/run/docker.sock </dev/null &
# Disable check until new version is tested.
# shellcheck disable=SC2064
trap "kill $!" EXIT

LOAD_COMMAND=
if [ -n "$DOCKER_IMAGE_PARENT" ]; then
    test -n "$DOCKER_IMAGE_PARENT_TASK" || raise_error "DOCKER_IMAGE_PARENT_TASK must be provided."
    LOAD_COMMAND="\
      /builds/worker/checkouts/gecko/mach taskcluster-load-image \
      --task-id \"$DOCKER_IMAGE_PARENT_TASK\" \
      -t \"$DOCKER_IMAGE_PARENT\" && "
fi

# Build image
run-task \
  --gecko-checkout "/builds/worker/checkouts/gecko" \
  --gecko-sparse-profile build/sparse-profiles/docker-image \
  -- \
  sh -x -c "$LOAD_COMMAND \
  /builds/worker/checkouts/gecko/mach taskcluster-build-image \
  -t \"${IMAGE_NAME}:${HASH}-pre\" \
  \"$IMAGE_NAME\""

# Squash the image
export DOCKER_HOST=unix:/$DOCKER_SOCKET
/usr/local/bin/docker-squash -v -t "${IMAGE_NAME}:${HASH}" "${IMAGE_NAME}:${HASH}-pre"

# Create artifact folder (note that this must occur after run-task)
mkdir -p /builds/worker/workspace/artifacts

# Get image from docker daemon (try up to 10 times)
# This interacts directly with the docker remote API, see:
# https://docs.docker.com/engine/reference/api/docker_remote_api_v1.18/
#
# The script will retry up to 10 times.
# Disable quoting error until fixing the / escaping
# shellcheck disable=SC2086
/usr/local/bin/download-and-compress \
    http+unix://%2Fvar%2Frun%2Fdocker.sock/images/${IMAGE_NAME}:${HASH}/get \
    /builds/worker/workspace/image.tar.zst.tmp \
    /builds/worker/workspace/artifacts/image.tar.zst
