#! /bin/bash -xe

set -x -e

echo "running as" $(id)

####
# Taskcluster friendly wrapper for performing fx Mac OSX tests via mozharness.
####

# Inputs, with defaults

: MOZHARNESS_URL                ${MOZHARNESS_URL}
: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}

WORKSPACE=$HOME
cd $WORKSPACE

rm -rf artifacts
mkdir artifacts

# test required parameters are supplied
if [[ -z ${MOZHARNESS_URL} ]]; then fail "MOZHARNESS_URL is not set"; fi
if [[ -z ${MOZHARNESS_SCRIPT} ]]; then fail "MOZHARNESS_SCRIPT is not set"; fi
if [[ -z ${MOZHARNESS_CONFIG} ]]; then fail "MOZHARNESS_CONFIG is not set"; fi

# Unzip the mozharness ZIP file created by the build task
if ! curl --fail -o mozharness.zip --retry 10 -L $MOZHARNESS_URL; then
    fail "failed to download mozharness zip"
fi
rm -rf mozharness
unzip -q mozharness.zip
rm mozharness.zip

# For telemetry purposes, the build process wants information about the
# source it is running; tc-vcs obscures this a little, but we can provide
# it directly.
export MOZ_SOURCE_REPO="${GECKO_HEAD_REPOSITORY}"
export MOZ_SOURCE_CHANGESET="${GECKO_HEAD_REV}"

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config-file ${cfg}"
done

rm -rf build logs properties target.dmg

# run the given mozharness script and configs, but pass the rest of the
# arguments in from our own invocation
python2.7 $WORKSPACE/mozharness/scripts/${MOZHARNESS_SCRIPT} ${config_cmds} "${@}"
