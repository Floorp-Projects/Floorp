#! /bin/bash -vex

set -x -e

echo "running as" $(id)

####
# Taskcluster friendly wrapper for running a script in
# testing/mozharness/scripts in a source checkout (no build).
# Example use: Python-only harness unit tests
####

: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}
: SRC_ROOT                      ${SRC_ROOT:=$WORKSPACE/build/src}
# These paths should be relative to $SRC_ROOT
: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}
: mozharness args               "${@}"

set -v
cd $WORKSPACE

fail() {
    echo # make sure error message is on a new line
    echo "[harness-test-linux.sh:error]" "${@}"
    exit 1
}

if [[ -z ${MOZHARNESS_SCRIPT} ]]; then fail "MOZHARNESS_SCRIPT is not set"; fi

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config-file ${SRC_ROOT}/${cfg}"
done

python2.7 $SRC_ROOT/${MOZHARNESS_SCRIPT} ${config_cmds} "${@}"



