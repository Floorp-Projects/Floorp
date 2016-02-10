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
: NEED_PULSEAUDIO               ${NEED_PULSEAUDIO:=false}
: NEED_PULL_GAIA                ${NEED_PULL_GAIA:=false}
: SKIP_MOZHARNESS_RUN           ${SKIP_MOZHARNESS_RUN:=false}
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
    Xvfb :0 -nolisten tcp -screen 0 1600x1200x24 \
       > ~/artifacts/public/xvfb.log 2>&1 &
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

gaia_cmds=""
if $NEED_PULL_GAIA; then
    # test required parameters are supplied
    if [[ -z ${GAIA_BASE_REPOSITORY} ]]; then exit 1; fi
    if [[ -z ${GAIA_HEAD_REPOSITORY} ]]; then exit 1; fi
    if [[ -z ${GAIA_REV} ]]; then exit 1; fi
    if [[ -z ${GAIA_REF} ]]; then exit 1; fi

    tc-vcs checkout \
        ${WORKSPACE}/gaia \
        ${GAIA_BASE_REPOSITORY} \
        ${GAIA_HEAD_REPOSITORY} \
        ${GAIA_REV} \
        ${GAIA_REF}

    gaia_cmds="--gaia-dir=${WORKSPACE}"
fi

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config-file ${cfg}"
done

if [ ${SKIP_MOZHARNESS_RUN} == true ]; then
  # Skipping Mozharness is to allow the developer start the window manager
  # properly and letting them change the execution of Mozharness without
  # exiting the container
  echo "We skipped running Mozharness."
  echo "Make sure you export DISPLAY=:0 before calling Mozharness."
  echo "Don't forget to call it with 'sudo -E -u worker'."
else
  # run the given mozharness script and configs, but pass the rest of the
  # arguments in from our own invocation
  python2.7 $WORKSPACE/${MOZHARNESS_SCRIPT} ${config_cmds} ${gaia_cmds} "${@}"
fi
