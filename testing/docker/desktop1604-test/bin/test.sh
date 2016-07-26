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

fail() {
    echo # make sure error message is on a new line
    echo "[test.sh:error]" "${@}"
    exit 1
}

####
# Now get the test-linux.sh/test-ubuntu.sh script from the given Gecko tree
# and run it with the same arguments.
####

[ -d $WORKSPACE ] || mkdir -p $WORKSPACE
cd $WORKSPACE

script=taskcluster/scripts/tester/test-ubuntu1604.sh
url=${GECKO_HEAD_REPOSITORY}/raw-file/${GECKO_HEAD_REV}/${script}
if ! curl --fail -o ./test-linux.sh --retry 10 $url; then
    fail "failed downloading test-ubuntu1604.sh from ${GECKO_HEAD_REPOSITORY}"
fi
chmod +x ./test-linux.sh
exec ./test-linux.sh "${@}"
