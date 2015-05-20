#! /bin/bash -vex

set -v -x -e

# Inputs, with defaults

: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}

: GECKO_BASE_REPOSITORY         ${GECKO_BASE_REPOSITORY:=https://hg.mozilla.org/mozilla-central}
: GECKO_HEAD_REPOSITORY         ${GECKO_HEAD_REPOSITORY:=https://hg.mozilla.org/mozilla-central}
: GECKO_REV                     ${GECKO_REV:=default}

: MOZHARNESS_BASE_REPOSITORY    ${MOZHARNESS_BASE_REPOSITORY:=https://hg.mozilla.org/build/mozharness}
: MOZHARNESS_HEAD_REPOSITORY    ${MOZHARNESS_HEAD_REPOSITORY:=https://hg.mozilla.org/build/mozharness}
: MOZHARNESS_REV                ${MOZHARNESS_REV:=production}

: TOOLS_BASE_REPOSITORY         ${TOOLS_BASE_REPOSITORY:=https://hg.mozilla.org/build/tools}
: TOOLS_HEAD_REPOSITORY         ${TOOLS_HEAD_REPOSITORY:=https://hg.mozilla.org/build/tools}
: TOOLS_REV                     ${TOOLS_REV:=default}

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}

: RELENGAPI_TOKEN               ${RELENGAPI_TOKEN+HIDDEN}

: NEED_XVFB                     ${NEED_XVFB:=false}

: MH_CUSTOM_BUILD_VARIANT_CFG   ${MH_CUSTOM_BUILD_VARIANT_CFG}
: MH_BRANCH                     ${MH_BRANCH:=mozilla-central}
: MH_BUILD_POOL                 ${MH_BUILD_POOL:=staging}

: MOZ_SIGNING_SERVERS           ${MOZ_SIGNING_SERVERS}
: MOZ_SIGN_CMD                  ${MOZ_SIGN_CMD}

: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}

# buildbot
export CCACHE_COMPRESS=1
export CCACHE_DIR=/builds/ccache
export CCACHE_HASHDIR=
export CCACHE_UMASK=002

export MOZ_AUTOMATION=1
export MOZ_CRASHREPORTER_NO_REPORT=1
export MOZ_OBJDIR=obj-firefox
export MOZ_SYMBOLS_EXTRA_BUILDID=linux64
export POST_SYMBOL_UPLOAD_CMD=/usr/local/bin/post-symbol-upload.py
export TINDERBOX_OUTPUT=1

# Ensure that in tree libraries can be found
export LIBRARY_PATH=$LIBRARY_PATH:$WORKSPACE/src/obj-firefox:$WORKSPACE/src/gcc/lib64

# test required parameters are supplied
test ${MOZHARNESS_SCRIPT}
test ${MOZHARNESS_CONFIG}

cleanup() {
    [ -n "$xvfb_pid" ] && kill $xvfb_pid
}
trap cleanup EXIT INT

# check out mozharness
tc-vcs checkout mozharness $MOZHARNESS_BASE_REPOSITORY $MOZHARNESS_HEAD_REPOSITORY $MOZHARNESS_REV

# check out tools where mozharness expects it to be ($PWD/build/tools and $WORKSPACE/build/tools)
tc-vcs checkout $WORKSPACE/build/tools $TOOLS_BASE_REPOSITORY $TOOLS_HEAD_REPOSITORY $TOOLS_REV
if [ ! -d build ]; then
    mkdir -p build
    ln -s $WORKSPACE/build/tools build/tools
fi

# and check out mozilla-central where mozharness will use it as a cache (/builds/hg-shared)
tc-vcs checkout /builds/hg-shared/mozilla-central $GECKO_BASE_REPOSITORY $GECKO_HEAD_REPOSITORY $GECKO_REV

# run mozharness in XVfb, if necessary; this is an array to maintain the quoting in the -s argument
if $NEED_XVFB; then
    # Some mozharness scripts set DISPLAY=:2
    Xvfb :2 -screen 0 1024x768x24 &
    export DISPLAY=:2
    xvfb_pid=$!
    # Only error code 255 matters, because it signifies that no
    # display could be opened. As long as we can open the display
    # tests should work.
    sleep 2 # we need to sleep so that Xvfb has time to startup
    xvinfo || if [ $? == 255 ]; then exit 255; fi
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

./${MOZHARNESS_SCRIPT} \
  --config ${MOZHARNESS_CONFIG} \
  $debug_flag \
  $custom_build_variant_cfg_flag \
  --disable-mock \
  --no-setup-mock \
  --no-clone-tools \
  --no-clobber \
  --no-update \
  --log-level=debug \
  --work-dir=$WORKSPACE/build \
  --no-action=generate-build-stats \
  --branch=${MH_BRANCH} \
  --build-pool=${MH_BUILD_POOL}

# if mozharness has created an "upload" directory, copy all of that into artifacts
if [ -d $WORKSPACE/build/upload ]; then
    cp -r $WORKSPACE/build/upload/* $HOME/artifacts/
fi
