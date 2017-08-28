#! /bin/bash -vex

set -x -e -v

# Relative path to in-tree script
: JOB_SCRIPT                ${JOB_SCRIPT:=taskcluster/scripts/builder/build-linux.sh}

script_args="${@}"

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
    exec su worker -c "/builds/worker/bin/build.sh $script_args"
fi

####
# The default build works for any fx_desktop_build based mozharness job:
# via build-linux.sh
####

. $HOME/bin/checkout-sources.sh

script=$WORKSPACE/build/src/$JOB_SCRIPT
chmod +x $script
exec $script $script_args
