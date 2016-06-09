#! /bin/bash -vex

set -x -e

echo "running as" $(id)

####
# Taskcluster friendly wrapper for performing fx desktop l10n repacks via mozharness.
# Based on ./build-linux.sh
####

# Inputs, with defaults

: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}
: MOZHARNESS_ACTIONS            ${MOZHARNESS_ACTIONS}
: MOZHARNESS_OPTIONS            ${MOZHARNESS_OPTIONS}

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}

: NEED_XVFB                     ${NEED_XVFB:=false}

: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}

set -v

fail() {
    echo # make sure error message is on a new line
    echo "[build-l10n.sh:error]" "${@}"
    exit 1
}

export MOZ_CRASHREPORTER_NO_REPORT=1
export MOZ_OBJDIR=obj-firefox
export TINDERBOX_OUTPUT=1

# Ensure that in tree libraries can be found
export LIBRARY_PATH=$LIBRARY_PATH:$WORKSPACE/src/obj-firefox:$WORKSPACE/src/gcc/lib64

# test required parameters are supplied
if [[ -z ${MOZHARNESS_SCRIPT} ]]; then fail "MOZHARNESS_SCRIPT is not set"; fi
if [[ -z ${MOZHARNESS_CONFIG} ]]; then fail "MOZHARNESS_CONFIG is not set"; fi

cleanup() {
    local rv=$?
    if [ -n "$xvfb_pid" ]; then
        kill $xvfb_pid || true
    fi
    exit $rv
}
trap cleanup EXIT INT

# run mozharness in XVfb, if necessary; this is an array to maintain the quoting in the -s argument
if $NEED_XVFB; then
    # Some mozharness scripts set DISPLAY=:2
    Xvfb :2 -screen 0 1024x768x24 &
    export DISPLAY=:2
    xvfb_pid=$!
    # Only error code 255 matters, because it signifies that no
    # display could be opened. As long as we can open the display
    # tests should work. We'll retry a few times with a sleep before
    # failing.
    retry_count=0
    max_retries=2
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
    if [ $xvfb_test == 255 ]; then fail "xvfb did not start properly"; fi
fi

# set up mozharness configuration, via command line, env, etc.

# $TOOLTOOL_CACHE bypasses mozharness completely and is read by tooltool_wrapper.sh to set the
# cache.  However, only some mozharness scripts use tooltool_wrapper.sh, so this may not be
# entirely effective.
export TOOLTOOL_CACHE

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config ${cfg}"
done

# if MOZHARNESS_ACTIONS is given, only run those actions (completely overriding default_actions
# in the mozharness configuration)
if [ -n "$MOZHARNESS_ACTIONS" ]; then
    actions=""
    for action in $MOZHARNESS_ACTIONS; do
        actions="$actions --$action"
    done
fi

# if MOZHARNESS_OPTIONS is given, append them to mozharness command line run
# e.g. enable-pgo
if [ -n "$MOZHARNESS_OPTIONS" ]; then
    options=""
    for option in $MOZHARNESS_OPTIONS; do
        options="$options --$option"
    done
fi

python2.7 $WORKSPACE/build/src/testing/${MOZHARNESS_SCRIPT} \
  --disable-mock \
  --revision ${GECKO_HEAD_REV} \
  $actions \
  $options \
  ${config_cmds} \
  --log-level=debug \
  --work-dir=$WORKSPACE/build \
