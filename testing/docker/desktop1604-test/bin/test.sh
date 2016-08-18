#! /bin/bash -vex

set -x -e

: GECKO_HEAD_REPOSITORY         ${GECKO_HEAD_REPOSITORY:=https://hg.mozilla.org/mozilla-central}
: GECKO_HEAD_REV                ${GECKO_HEAD_REV:=default}
: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}


# TODO: when bug 1093833 is solved and tasks can run as non-root, reduce this
# to a simple fail-if-root check
if [ $(id -u) = 0 ]; then
    chown -R worker:worker /home/worker
    # drop privileges by re-running this script
    exec sudo -E -u worker bash /home/worker/bin/test.sh "${@}"
fi

[ -d $WORKSPACE ] || mkdir -p $WORKSPACE
cd $WORKSPACE
exec /home/worker/bin/test-linux.sh "${@}"
