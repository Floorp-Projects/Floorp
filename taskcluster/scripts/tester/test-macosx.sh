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

# Download mozharness with exponential backoff
# curl already applies exponential backoff, but not for all
# failed cases, apparently, as we keep getting failed downloads
# with 404 code.
download_mozharness() {
    local max_attempts=10
    local timeout=1
    local attempt=0

    echo "Downloading mozharness"

    while [[ $attempt < $max_attempts ]]; do
        if curl --fail -o mozharness.zip --retry 10 -L $MOZHARNESS_URL; then
            rm -rf mozharness
            if unzip -q mozharness.zip; then
                return 0
            fi
            echo "error unzipping mozharness.zip" >&2
        else
            echo "failed to download mozharness zip" >&2
        fi
        echo "Download failed, retrying in $timeout seconds..." >&2
        sleep $timeout
        timeout=$((timeout*2))
        attempt=$((attempt+1))
    done

    fail "Failed to download and unzip mozharness"
}

download_mozharness
rm mozharness.zip

# For telemetry purposes, the build process wants information about the
# source it is running.
export MOZ_SOURCE_REPO="${GECKO_HEAD_REPOSITORY}"
export MOZ_SOURCE_CHANGESET="${GECKO_HEAD_REV}"

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config-file ${cfg}"
done

rm -rf build logs properties target.dmg

# Use |mach python| if a source checkout exists so in-tree packages are
# available.
[[ -d "${GECKO_PATH}" ]] && python="${GECKO_PATH}/mach python" || python="python2.7"

# run the given mozharness script and configs, but pass the rest of the
# arguments in from our own invocation
$python $WORKSPACE/mozharness/scripts/${MOZHARNESS_SCRIPT} ${config_cmds} "${@}"
