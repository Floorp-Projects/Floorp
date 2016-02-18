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

####
# Now get the test-linux.sh script from the given Gecko tree and run it with
# the same arguments.
####

[ -d $WORKSPACE ] || mkdir -p $WORKSPACE
cd $WORKSPACE

script=testing/taskcluster/scripts/tester/test-b2g.sh
url=${GECKO_HEAD_REPOSITORY}/raw-file/${GECKO_HEAD_REV}/${script}
curl --fail -o ./test-b2g.sh --retry 10 $url
chmod +x ./test-b2g.sh
exec ./test-b2g.sh "${@}"

