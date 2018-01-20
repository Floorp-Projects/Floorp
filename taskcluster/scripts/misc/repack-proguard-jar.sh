#!/bin/bash
set -x -e -v

# This script is for repacking Proguard to get the `proguard.jar`
# needed to shrink Android packages.

WORKSPACE=$HOME/workspace
STAGE=$WORKSPACE/proguard
UPLOAD_DIR=$HOME/artifacts

VERSION=5.3.3
URL=https://newcontinuum.dl.sourceforge.net/project/proguard/proguard/5.3/proguard$VERSION.tar.gz
ARCHIVE=proguard$VERSION.tar.gz
DIR=proguard$VERSION
SHA256SUM=95bf9580107f00d0e26f01026dcfe9e7a772e5449488b03ba832836c3760b3af

mkdir -p $UPLOAD_DIR $STAGE

cd $WORKSPACE
wget --progress=dot:mega $URL
echo "$SHA256SUM  $ARCHIVE" | sha256sum -c -

# Just the file we need.
tar --wildcards -zxvf $ARCHIVE '*/proguard.jar'

# The archive is to satisfy source distribution requirements.
mv $ARCHIVE $UPLOAD_DIR

# This leaves us with $STAGE/lib/proguard.jar.
mv $DIR/lib $STAGE

cat >$STAGE/README<<EOF
proguard.jar extracted from ${URL}.
That archive, which includes source, is available as a taskcluster artifact:
https://queue.taskcluster.net/v1/task/$TASK_ID/artifacts/public/$ARCHIVE
EOF
tar cf - -C $WORKSPACE `basename $STAGE` | xz > $UPLOAD_DIR/proguard-jar.tar.xz
