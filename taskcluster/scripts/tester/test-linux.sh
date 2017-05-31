#! /bin/bash -xe

set -x -e

echo "running as" $(id)

# Detect release version.
. /etc/lsb-release
if [ "${DISTRIB_RELEASE}" == "12.04" ]; then
    UBUNTU_1204=1
elif [ "${DISTRIB_RELEASE}" == "16.04" ]; then
    UBUNTU_1604=1
fi

####
# Taskcluster friendly wrapper for performing fx desktop tests via mozharness.
####

# Inputs, with defaults

: MOZHARNESS_PATH               ${MOZHARNESS_PATH}
: MOZHARNESS_URL                ${MOZHARNESS_URL}
: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}
: NEED_XVFB                     ${NEED_XVFB:=true}
: NEED_WINDOW_MANAGER           ${NEED_WINDOW_MANAGER:=false}
: NEED_PULSEAUDIO               ${NEED_PULSEAUDIO:=false}
: START_VNC                     ${START_VNC:=false}
: TASKCLUSTER_INTERACTIVE       ${TASKCLUSTER_INTERACTIVE:=false}
: WORKSPACE                     ${WORKSPACE:=$HOME/workspace}
: mozharness args               "${@}"

set -v
mkdir -p $WORKSPACE
cd $WORKSPACE

fail() {
    echo # make sure error message is on a new line
    echo "[test-linux.sh:error]" "${@}"
    exit 1
}

maybe_start_pulse() {
    if $NEED_PULSEAUDIO; then
        pulseaudio --fail --daemonize --start
        pactl load-module module-null-sink
    fi
}

# test required parameters are supplied
if [ -z "${MOZHARNESS_PATH}" -a -z "${MOZHARNESS_URL}" ]; then
    fail "MOZHARNESS_PATH or MOZHARNESS_URL must be defined";
fi

if [[ -z ${MOZHARNESS_SCRIPT} ]]; then fail "MOZHARNESS_SCRIPT is not set"; fi
if [[ -z ${MOZHARNESS_CONFIG} ]]; then fail "MOZHARNESS_CONFIG is not set"; fi

# make sure artifact directories exist
mkdir -p $WORKSPACE/build/upload/logs
mkdir -p ~/artifacts/public
mkdir -p $WORKSPACE/build/blobber_upload_dir

cleanup() {
    local rv=$?
    if [[ -s $HOME/.xsession-errors ]]; then
      # To share X issues
      cp $HOME/.xsession-errors ~/artifacts/public/xsession-errors.log
    fi
    if $NEED_XVFB; then
        cleanup_xvfb
    fi
    exit $rv
}
trap cleanup EXIT INT

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

# Download mozharness if we're told to.
if [ ${MOZHARNESS_URL} ]; then
    download_mozharness
    rm mozharness.zip

    if ! [ -d mozharness ]; then
        fail "mozharness zip did not contain mozharness/"
    fi

    MOZHARNESS_PATH=`pwd`/mozharness
fi

# pulseaudio daemon must be started before xvfb on Ubuntu 12.04.
if [ "${UBUNTU_1204}" ]; then
    maybe_start_pulse
fi

# run XVfb in the background, if necessary
if $NEED_XVFB; then
    # note that this file is not available when run under native-worker
    . $HOME/scripts/xvfb.sh
    start_xvfb '1600x1200x24' 0
fi

if $START_VNC; then
    x11vnc > ~/artifacts/public/x11vnc.log 2>&1 &
fi

if $NEED_WINDOW_MANAGER; then
    # This is read by xsession to select the window manager
    echo DESKTOP_SESSION=ubuntu > $HOME/.xsessionrc

    # note that doing anything with this display before running Xsession will cause sadness (like,
    # crashes in compiz). Make sure that X has enough time to start
    sleep 15
    # DISPLAY has already been set above
    # XXX: it would be ideal to add a semaphore logic to make sure that the
    # window manager is ready
    /etc/X11/Xsession 2>&1 &

    # Turn off the screen saver and screen locking
    gsettings set org.gnome.desktop.screensaver idle-activation-enabled false
    gsettings set org.gnome.desktop.screensaver lock-enabled false
    gsettings set org.gnome.desktop.screensaver lock-delay 3600
    # Disable the screen saver
    xset s off s reset

    if [ "${UBUNTU_1604}" ]; then
        # start compiz for our window manager
        compiz 2>&1 &
        #TODO: how to determine if compiz starts correctly?
    fi
fi

if [ "${UBUNTU_1604}" ]; then
    maybe_start_pulse
fi

# For telemetry purposes, the build process wants information about the
# source it is running; tc-vcs obscures this a little, but we can provide
# it directly.
export MOZ_SOURCE_REPO="${GECKO_HEAD_REPOSITORY}"
export MOZ_SOURCE_CHANGESET="${GECKO_HEAD_REV}"

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config-file ${MOZHARNESS_PATH}/configs/${cfg}"
done

mozharness_bin="$HOME/bin/run-mozharness"
mkdir -p $(dirname $mozharness_bin)

# Save the computed mozharness command to a binary which is useful
# for interactive mode.
echo -e "#!/usr/bin/env bash
# Some mozharness scripts assume base_work_dir is in
# the current working directory, see bug 1279237
cd $WORKSPACE
cmd=\"python2.7 ${MOZHARNESS_PATH}/scripts/${MOZHARNESS_SCRIPT} ${config_cmds} ${@} \${@}\"
echo \"Running: \${cmd}\"
exec \${cmd}" > ${mozharness_bin}
chmod +x ${mozharness_bin}

# In interactive mode, the user will be prompted with options for what to do.
if ! $TASKCLUSTER_INTERACTIVE; then
  # run the given mozharness script and configs, but pass the rest of the
  # arguments in from our own invocation
  ${mozharness_bin};
fi

# Run a custom mach command (this is typically used by action tasks to run
# harnesses in a particular way)
if [ "$CUSTOM_MACH_COMMAND" ]; then
    eval "$HOME/workspace/build/tests/mach ${CUSTOM_MACH_COMMAND}"
    exit $?
fi
