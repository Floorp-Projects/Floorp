#!/bin/bash -vex

gecko_dir=$1
test -d $gecko_dir

if [ ! -d "$gecko_dir/gcc" ]; then
  cd $gecko_dir
  curl https://s3-us-west-2.amazonaws.com/test-caching/packages/gcc.tar.xz | tar Jx
  cd -
fi

if [ ! -d "$gecko_dir/sccache" ]; then
  cd $gecko_dir
  curl https://s3-us-west-2.amazonaws.com/test-caching/packages/sccache.tar.bz2 | tar jx
  cd -
fi

# Remove cached moztt directory if it exists when a user supplied a git url/revision
if [ ! -z $MOZTT_GIT_URL ] || [ ! -z $MOZTT_REVISION ]; then
  echo "Removing cached moztt package"
  rm -rf moztt
fi

moztt_url=${MOZTT_GIT_URL:=https://github.com/mozilla-b2g/moztt}
moztt_revision=${MOZTT_REVISION:=master}

tc-vcs checkout $gecko_dir/moztt $moztt_url $moztt_url $moztt_revision
