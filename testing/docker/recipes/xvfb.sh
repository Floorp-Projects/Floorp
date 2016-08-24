#! /bin/bash -x

set -x

fail() {
    echo # make sure error message is on a new line
    echo "[xvfb.sh:error]" "${@}"
    exit 1
}

cleanup_xvfb() {
    # When you call this script with START_VNC or TASKCLUSTER_INTERACTIVE
    # we make sure we do not kill xvfb so you do not lose your connection
    local xvfb_pid=`pidof Xvfb`
    local vnc=${START_VNC:-false}
    local interactive=${TASKCLUSTER_INTERACTIVE:-false}
    if [ -n "$xvfb_pid" ] && [[ $vnc == false ]] && [[ $interactive == false ]] ; then
        kill $xvfb_pid || true
        screen -XS xvfb quit || true
    fi
}

# Attempt to start xvfb in a screen session with the given resolution and display
# number.  Up to 5 attempts will be made to start xvfb with a short delay
# between retries
try_xvfb() {
    screen -dmS xvfb Xvfb :$2 -nolisten tcp -screen 0 $1 \
       > ~/artifacts/xvfb/xvfb.log 2>&1
    export DISPLAY=:$2

    # Only error code 255 matters, because it signifies that no
    # display could be opened. As long as we can open the display
    # tests should work. We'll retry a few times with a sleep before
    # failing.
    local retry_count=0
    local max_retries=5
    xvfb_test=0
    until [ $retry_count -gt $max_retries ]; do
        xvinfo || xvfb_test=$?
        if [ $xvfb_test != 255 ]; then
            retry_count=$(($max_retries + 1))
        else
            retry_count=$(($retry_count + 1))
            echo "Failed to start Xvfb, retry: $retry_count"
            sleep 2
        fi
    done
    if [ $xvfb_test == 255 ]; then
        return 1
    else
        return 0
    fi
}

start_xvfb() {
    set +e
    mkdir -p ~/artifacts/xvfb
    local retry_count=0
    local max_retries=2
    local success=1
    until [ $retry_count -gt $max_retries ]; do
        try_xvfb $1 $2
        success=$?
        if [ $success -eq 0 ]; then
            retry_count=$(($max_retries + 1))
        else
            retry_count=$(($retry_count + 1))
            sleep 10
        fi
    done
    set -e
    if [ $success -eq 1 ]; then
        fail "Could not start xvfb after ${max_retries} attempts"
    fi
}
