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
: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}
: mozharness args               "${@}"

set -v
cd $WORKSPACE

# test required parameters are supplied
if [[ -z ${MOZHARNESS_URL} ]]; then exit 1; fi
if [[ -z ${MOZHARNESS_SCRIPT} ]]; then exit 1; fi
if [[ -z ${MOZHARNESS_CONFIG} ]]; then exit 1; fi

mkdir -p ~/artifacts/public

cleanup() {
    if [ -n "$xvfb_pid" ]; then
        kill $xvfb_pid || true
    fi
}
trap cleanup EXIT INT

# Unzip the mozharness ZIP file created by the build task
curl --fail -o mozharness.zip --retry 10 -L $MOZHARNESS_URL
rm -rf mozharness
unzip -q mozharness.zip
rm mozharness.zip

if ! [ -d mozharness ]; then
    echo "mozharness zip did not contain mozharness/"
    exit 1
fi

# start up the pulseaudio daemon.  Note that it's important this occur
# before the Xvfb startup.
if $NEED_PULSEAUDIO; then
    pulseaudio --fail --daemonize --start
    pactl load-module module-null-sink
fi

# run XVfb in the background, if necessary
if $NEED_XVFB; then
    Xvfb :0 -nolisten tcp -screen 0 1600x1200x24 &
    export DISPLAY=:0
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
        fi done
    if [ $xvfb_test == 255 ]; then exit 255; fi
fi

if $NEED_WINDOW_MANAGER; then
    # start up the gnome session, using a semaphore file to know when the session has fully started
    # (at least to the point where apps are automatically started)
    semaphore=/tmp/gnome-session-started
    rm -f $semaphore
    mkdir -p ~/.config/autostart
    cat <<EOF > ~/.config/autostart/gnome-started.desktop
[Desktop Entry]
Type=Application
Exec=touch $semaphore
Hidden=false
X-GNOME-Autostart-enabled=true
Name=Startup Complete
Comment=Notify test-linux.sh that GNOME session startup is complete
StartupNotify=false
Terminal=false
Type=Application
EOF
    gnome-session > ~/artifacts/public/gnome-session.log 2>&1 &
    while [ ! -f $semaphore ]; do
        sleep 1
    done
fi

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config-file ${cfg}"
done

# run the given mozharness script and configs, but pass the rest of the
# arguments in from our own invocation
python2.7 $WORKSPACE/${MOZHARNESS_SCRIPT} ${config_cmds} "${@}"

