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
test -n "$PROJECT" || raise_error "Project must be provided."
test -n "$HASH" || raise_error "Context Hash must be provided."

mkdir /artifacts

if [ ! -z "$CONTEXT_URL" ]; then
    mkdir /context
    if ! curl -L --retry 5 --connect-timeout 30 --fail "$CONTEXT_URL" | tar -xz --strip-components 1 -C /context; then
        raise_error "Error downloading image context from decision task."
    fi
    CONTEXT_PATH=/context
else
    tc-vcs checkout /home/worker/workspace/src $BASE_REPOSITORY $HEAD_REPOSITORY $HEAD_REV $HEAD_REF
    CONTEXT_PATH=/home/worker/workspace/src/$CONTEXT_PATH
fi

test -d $CONTEXT_PATH || raise_error "Context Path $CONTEXT_PATH does not exist."
test -f "$CONTEXT_PATH/Dockerfile" || raise_error "Dockerfile must be present in $CONTEXT_PATH."

docker build -t $PROJECT:$HASH $CONTEXT_PATH
docker save $PROJECT:$HASH > /artifacts/image.tar
