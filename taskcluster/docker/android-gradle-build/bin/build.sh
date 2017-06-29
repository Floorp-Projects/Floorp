#! /bin/bash -vex

set -x -e -v

# TODO: when bug 1093833 is solved and tasks can run as non-root, reduce this
# to a simple fail-if-root check
if [ $(id -u) = 0 ]; then
    # each of the caches we have mounted are owned by root, so update that ownership
    # to 'worker'
    for cache in /builds/worker/.tc-vcs /builds/worker/workspace /builds/worker/tooltool-cache; do
        if [ -d $cache ]; then
            # -R probably isn't necessary forever, but it fixes some poisoned
            # caches for now
            chown -R worker:worker $cache
        fi
    done

    # ..then drop privileges by re-running this script
    exec su worker /builds/worker/bin/build.sh
fi

####
# The default build works for any fx_desktop_build based mozharness job:
# via linux-build.sh
####

. $HOME/bin/checkout-sources.sh

. $WORKSPACE/build/src/taskcluster/scripts/builder/build-linux.sh
