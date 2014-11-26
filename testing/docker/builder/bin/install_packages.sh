#!/bin/bash -vex

if [ ! -d "gcc" ]; then
    curl https://s3-us-west-2.amazonaws.com/test-caching/packages/gcc.tar.xz | tar Jx
fi

if [ ! -d "sccache" ]; then
    curl https://s3-us-west-2.amazonaws.com/test-caching/packages/sccache.tar.bz2 | tar jx
fi

# Remove cached moztt directory if it exists when a user supplied a git url/revision
if [ ! -z $MOZTT_GIT_URL ] || [ ! -z $MOZTT_REVISION ]; then
    echo "Removing cached moztt package"
    rm -rf moztt
fi

if [ ! -d "moztt" ]; then
    moztt_url=${MOZTT_GIT_URL:=https://github.com/mozilla-b2g/moztt}
    moztt_revision=${MOZTT_REVISION:=master}
    git clone $moztt_url moztt
    cd moztt && git checkout $moztt_revision
    echo "moztt repository: $moztt_url"
    echo "moztt revision: $(git rev-parse HEAD)"
    cd $gecko_dir
fi

