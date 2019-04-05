#! /bin/bash -vex

set -x -e

echo "running as" $(id)

. /builds/worker/scripts/xvfb.sh

####
# Taskcluster friendly wrapper for performing fx desktop builds via mozharness.
####

# Inputs, with defaults

: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}
: MOZHARNESS_CONFIG_PATHS       ${MOZHARNESS_CONFIG_PATHS}
: MOZHARNESS_ACTIONS            ${MOZHARNESS_ACTIONS}
: MOZHARNESS_OPTIONS            ${MOZHARNESS_OPTIONS}

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/builds/worker/tooltool-cache}

: WORKSPACE                     ${WORKSPACE:=/builds/worker/workspace}

set -v

fail() {
    echo # make sure error message is on a new line
    echo "[build-linux.sh:error]" "${@}"
    exit 1
}

export MOZ_CRASHREPORTER_NO_REPORT=1
export MOZ_OBJDIR=obj-firefox
export TINDERBOX_OUTPUT=1

# use "simple" package names so that they can be hard-coded in the task's
# extras.locations
export MOZ_SIMPLE_PACKAGE_NAME=target

# test required parameters are supplied
if [[ -z ${MOZHARNESS_SCRIPT} ]]; then fail "MOZHARNESS_SCRIPT is not set"; fi
if [[ -z ${MOZHARNESS_CONFIG} ]]; then fail "MOZHARNESS_CONFIG is not set"; fi

cleanup() {
    local rv=$?
    cleanup_xvfb
    exit $rv
}
trap cleanup EXIT INT

# set up mozharness configuration, via command line, env, etc.

debug_flag=""
if [ 0$DEBUG -ne 0 ]; then
  debug_flag='--debug'
fi

# $TOOLTOOL_CACHE bypasses mozharness completely and is read by tooltool_wrapper.sh to set the
# cache.  However, only some mozharness scripts use tooltool_wrapper.sh, so this may not be
# entirely effective.
export TOOLTOOL_CACHE

config_path_cmds=""
for path in ${MOZHARNESS_CONFIG_PATHS}; do
    config_path_cmds="${config_path_cmds} --extra-config-path ${WORKSPACE}/build/src/${path}"
done

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

cd /builds/worker

$GECKO_PATH/mach python $GECKO_PATH/testing/${MOZHARNESS_SCRIPT} \
  ${config_path_cmds} \
  ${config_cmds} \
  $actions \
  $options \
  --log-level=debug \
  --work-dir=$WORKSPACE/build \
