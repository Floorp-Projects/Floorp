#! /bin/bash -vex

set -x -e

echo "running as" $(id)

####
# Taskcluster friendly wrapper for performing fx desktop builds via mozharness.
####

# Inputs, with defaults

: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}

: RELENGAPI_TOKEN               ${RELENGAPI_TOKEN+HIDDEN}

: NEED_XVFB                     ${NEED_XVFB:=false}

: MH_CUSTOM_BUILD_VARIANT_CFG   ${MH_CUSTOM_BUILD_VARIANT_CFG}
: MH_BRANCH                     ${MH_BRANCH:=mozilla-central}
: MH_BUILD_POOL                 ${MH_BUILD_POOL:=staging}

: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}

# files to be "uploaded" (moved to ~/artifacts) from obj-firefox/dist
: DIST_UPLOADS                  ${DIST_UPLOADS:=""}
# files which will be be prefixed with target before being sent to artifacts
# e.g. DIST_TARGET_UPLOADS="a.zip" runs mv v2.0.a.zip mv artifacts/target.a.zip
: DIST_TARGET_UPLOADS           ${DIST_TARGET_UPLOADS:=""}

set -v

# Don't run the upload step; this is passed through mozharness to mach.  Once
# the mozharness scripts are not run in Buildbot anymore, this can be moved to
# Mozharness (or the upload step removed from mach entirely)
export MOZ_AUTOMATION_UPLOAD=0

export MOZ_CRASHREPORTER_NO_REPORT=1
export MOZ_OBJDIR=obj-firefox
export MOZ_SYMBOLS_EXTRA_BUILDID=linux64
export POST_SYMBOL_UPLOAD_CMD=/usr/local/bin/post-symbol-upload.py
export TINDERBOX_OUTPUT=1

# Ensure that in tree libraries can be found
export LIBRARY_PATH=$LIBRARY_PATH:$WORKSPACE/src/obj-firefox:$WORKSPACE/src/gcc/lib64

# test required parameters are supplied
if [[ -z ${MOZHARNESS_SCRIPT} ]]; then exit 1; fi
if [[ -z ${MOZHARNESS_CONFIG} ]]; then exit 1; fi

cleanup() {
    if [ -n "$xvfb_pid" ]; then
        kill $xvfb_pid || true
    fi
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
        fi done
    if [ $xvfb_test == 255 ]; then exit 255; fi
fi

# set up mozharness configuration, via command line, env, etc.

debug_flag=""
if [ 0$DEBUG -ne 0 ]; then
  debug_flag='--debug'
fi

custom_build_variant_cfg_flag=""
if [ -n "${MH_CUSTOM_BUILD_VARIANT_CFG}" ]; then
    custom_build_variant_cfg_flag="--custom-build-variant-cfg=${MH_CUSTOM_BUILD_VARIANT_CFG}"
fi

set +x
# mozharness scripts look for the relengapi token at this location, so put it there,
# if specified
if [ -n "${RELENGAPI_TOKEN}" ]; then
    echo 'Storing $RELENGAPI_TOKEN in /builds/relengapi.tok'
    echo ${RELENGAPI_TOKEN} > /builds/relengapi.tok
    # unset it so that mozharness doesn't "helpfully" log it
    unset RELENGAPI_TOKEN
fi
set -x

# $TOOLTOOL_CACHE bypasses mozharness completely and is read by tooltool_wrapper.sh to set the
# cache.  However, only some mozharness scripts use tooltool_wrapper.sh, so this may not be
# entirely effective.
export TOOLTOOL_CACHE

# support multiple, space delimited, config files
config_cmds=""
for cfg in $MOZHARNESS_CONFIG; do
  config_cmds="${config_cmds} --config ${cfg}"
done

# Mozharness would ordinarily do the checkouts itself, but they are disabled
# here (--no-checkout-sources, --no-clone-tools) as the checkout is performed above.

python2.7 $WORKSPACE/build/src/testing/${MOZHARNESS_SCRIPT} ${config_cmds} \
  $debug_flag \
  $custom_build_variant_cfg_flag \
  --disable-mock \
  --no-setup-mock \
  --no-checkout-sources \
  --no-clone-tools \
  --no-clobber \
  --no-update \
  --no-upload-files \
  --no-sendchange \
  --log-level=debug \
  --work-dir=$WORKSPACE/build \
  --no-action=generate-build-stats \
  --branch=${MH_BRANCH} \
  --build-pool=${MH_BUILD_POOL}

mkdir -p /home/worker/artifacts

# upload auxiliary files
cd $WORKSPACE/build/src/obj-firefox/dist

for file in $DIST_UPLOADS
do
    mv $file $HOME/artifacts/$file
done

# Discard version numbers from packaged files, they just make it hard to write
# the right filename in the task payload where artifacts are declared
for file in $DIST_TARGET_UPLOADS
do
    mv *.$file $HOME/artifacts/target.$file
done
