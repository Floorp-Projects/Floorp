#! /bin/bash -xe

set -x -e

echo "running as" $(id)

####
# Taskcluster friendly wrapper for performing fx desktop tests via mozharness.
####

# Inputs, with defaults

: MOZHARNESS_URL                ${MOZHARNESS_URL}
: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}
: NEED_XVFB                     ${NEED_XVFB:=true}
: NEED_WINDOW_MANAGER           ${NEED_WINDOW_MANAGER:=false}
: NEED_PULSEAUDIO               ${NEED_PULSEAUDIO:=false}
: START_VNC                     ${START_VNC:=false}
: TASKCLUSTER_INTERACTIVE       ${TASKCLUSTER_INTERACTIVE:=false}
: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}
: mozharness args               "${@}"

set -v
cd $WORKSPACE

fail() {
    echo # make sure error message is on a new line
    echo "[test-linux.sh:error]" "${@}"
    exit 1
}

# test required parameters are supplied
if [[ -z ${MOZHARNESS_URL} ]]; then fail "MOZHARNESS_URL is not set"; fi
if [[ -z ${MOZHARNESS_SCRIPT} ]]; then fail "MOZHARNESS_SCRIPT is not set"; fi
if [[ -z ${MOZHARNESS_CONFIG} ]]; then fail "MOZHARNESS_CONFIG is not set"; fi

mkdir -p ~/artifacts/public

cleanup() {
    local rv=$?
    if [[ -s /home/worker/.xsession-errors ]]; then
      # To share X issues
      cp /home/worker/.xsession-errors ~/artifacts/public/xsession-errors.log
    fi
    # When you call this script with START_VNC or TASKCLUSTER_INTERACTIVE
    # we make sure we don't kill xvfb so you don't lose your connection
    xvfb_pid=`pidof Xvfb`
    if [ -n "$xvfb_pid" ] && [ $START_VNC == false ] && [ $TASKCLUSTER_INTERACTIVE == false ] ; then
        kill $xvfb_pid || true
        screen -XS xvfb quit || true
    fi
    exit $rv
}
trap cleanup EXIT INT

# Unzip the mozharness ZIP file created by the build task
if ! curl --fail -o mozharness.zip --retry 10 -L $MOZHARNESS_URL; then
    fail "failed to download mozharness zip"
fi
rm -rf mozharness
unzip -q mozharness.zip
rm mozharness.zip

if ! [ -d mozharness ]; then
    fail "mozharness zip did not contain mozharness/"
fi

# start up the pulseaudio daemon.  Note that it's important this occur
# before the Xvfb startup for ubuntu 12.04, not for 16.04
if $NEED_PULSEAUDIO; then
    pulseaudio --fail --daemonize --start
    pactl load-module module-null-sink
fi

# run Xvfb in the background, if necessary
if $NEED_XVFB; then
    screen -dmS xvfb Xvfb :0 -nolisten tcp -screen 0 1600x1200x24 \
       > ~/artifacts/public/xvfb.log 2>&1
    export DISPLAY=:0
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

if $START_VNC; then
    x11vnc > ~/artifacts/public/x11vnc.log 2>&1 &
fi

if $NEED_WINDOW_MANAGER; then
    # This is read by xsession to select the window manager
    echo DESKTOP_SESSION=ubuntu > /home/worker/.xsessionrc

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
fi

# For telemetry purposes, the build process wants information about the
# source it is running; tc-vcs obscures this a little, but we can provide
# it directly.
export MOZ_SOURCE_REPO="${GECKO_HEAD_REPOSITORY}"
export MOZ_SOURCE_CHANGESET="${GECKO_HEAD_REV}"

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config-file $WORKSPACE/${cfg}"
done

mozharness_bin="/home/worker/bin/run-mozharness"

# Save the computed mozharness command to a binary which is useful
# for interactive mode.
echo -e "#!/usr/bin/env bash
# Some mozharness scripts assume base_work_dir is in
# the current working directory, see bug 1279237
cd $WORKSPACE
cmd=\"python2.7 $WORKSPACE/${MOZHARNESS_SCRIPT} ${config_cmds} ${@} \${@}\"
echo \"Running: \${cmd}\"
exec \${cmd}" > ${mozharness_bin}
chmod +x ${mozharness_bin}

# In interactive mode, the user will be prompted with options for what to do.
if ! $TASKCLUSTER_INTERACTIVE; then
  # run the given mozharness script and configs, but pass the rest of the
  # arguments in from our own invocation
  ${mozharness_bin};
fi
