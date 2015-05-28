#! /bin/bash -vex

set -x -e

# Inputs, with defaults

: MOZHARNESS_SCRIPT             ${MOZHARNESS_SCRIPT}
: MOZHARNESS_CONFIG             ${MOZHARNESS_CONFIG}

# mozharness builds use three repositories: gecko (source), mozharness (build
# scripts) and tools (miscellaneous) for each, specify *_REPOSITORY.  If the
# revision is not in the standard repo for the codebase, specify *_BASE_REPO as
# the canonical repo to clone and *_HEAD_REPO as the repo containing the
# desired revision.  For Mercurial clones, only *_HEAD_REV is required; for Git
# clones, specify the branch name to fetch as *_HEAD_REF and the desired sha1
# as *_HEAD_REV.  For compatibility, we also accept MOZHARNESS_{REV,REF}

: GECKO_REPOSITORY              ${GECKO_REPOSITORY:=https://hg.mozilla.org/mozilla-central}
: GECKO_BASE_REPOSITORY         ${GECKO_BASE_REPOSITORY:=${GECKO_REPOSITORY}}
: GECKO_HEAD_REPOSITORY         ${GECKO_HEAD_REPOSITORY:=${GECKO_REPOSITORY}}
: GECKO_HEAD_REV                ${GECKO_HEAD_REV:=default}
: GECKO_HEAD_REF                ${GECKO_HEAD_REF:=${GECKO_HEAD_REV}}

: MOZHARNESS_REPOSITORY         ${MOZHARNESS_REPOSITORY:=https://hg.mozilla.org/build/mozharness}
: MOZHARNESS_BASE_REPOSITORY    ${MOZHARNESS_BASE_REPOSITORY:=${MOZHARNESS_REPOSITORY}}
: MOZHARNESS_HEAD_REPOSITORY    ${MOZHARNESS_HEAD_REPOSITORY:=${MOZHARNESS_REPOSITORY}}
: MOZHARNESS_REV                ${MOZHARNESS_REV:=production}
: MOZHARNESS_REF                ${MOZHARNESS_REF:=${MOZHARNESS_REV}}
: MOZHARNESS_HEAD_REV           ${MOZHARNESS_HEAD_REV:=${MOZHARNESS_REV}}
: MOZHARNESS_HEAD_REF           ${MOZHARNESS_HEAD_REF:=${MOZHARNESS_REF}}

: TOOLS_REPOSITORY              ${TOOLS_REPOSITORY:=https://hg.mozilla.org/build/tools}
: TOOLS_BASE_REPOSITORY         ${TOOLS_BASE_REPOSITORY:=${TOOLS_REPOSITORY}}
: TOOLS_HEAD_REPOSITORY         ${TOOLS_HEAD_REPOSITORY:=${TOOLS_REPOSITORY}}
: TOOLS_HEAD_REV                ${TOOLS_HEAD_REV:=default}
: TOOLS_HEAD_REF                ${TOOLS_HEAD_REF:=${TOOLS_HEAD_REV}}

: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}

: RELENGAPI_TOKEN               ${RELENGAPI_TOKEN+HIDDEN}

: NEED_XVFB                     ${NEED_XVFB:=false}

: MH_CUSTOM_BUILD_VARIANT_CFG   ${MH_CUSTOM_BUILD_VARIANT_CFG}
: MH_BRANCH                     ${MH_BRANCH:=mozilla-central}
: MH_BUILD_POOL                 ${MH_BUILD_POOL:=staging}

: MOZ_SIGNING_SERVERS           ${MOZ_SIGNING_SERVERS}
: MOZ_SIGN_CMD                  ${MOZ_SIGN_CMD}

: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}

set -v

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
if [[ -z ${MOZHARNESS_SCRIPT} ]]; then exit 1; fi
if [[ -z ${MOZHARNESS_CONFIG} ]]; then exit 1; fi

cleanup() {
    [ -n "$xvfb_pid" ] && kill $xvfb_pid
}
trap cleanup EXIT INT

# check out mozharness
tc-vcs checkout mozharness $MOZHARNESS_BASE_REPOSITORY $MOZHARNESS_HEAD_REPOSITORY $MOZHARNESS_HEAD_REV $MOZHARNESS_HEAD_REF

# check out tools where mozharness expects it to be ($PWD/build/tools and $WORKSPACE/build/tools)
tc-vcs checkout $WORKSPACE/build/tools $TOOLS_BASE_REPOSITORY $TOOLS_HEAD_REPOSITORY $TOOLS_HEAD_REV $TOOLS_HEAD_REF
if [ ! -d build ]; then
    mkdir -p build
    ln -s $WORKSPACE/build/tools build/tools
fi

# and check out mozilla-central where mozharness will use it as a cache (/builds/hg-shared)
tc-vcs checkout $WORKSPACE/build/src $GECKO_BASE_REPOSITORY $GECKO_HEAD_REPOSITORY $GECKO_HEAD_REV $GECKO_HEAD_REF

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

./${MOZHARNESS_SCRIPT} ${config_cmds} \
  $debug_flag \
  $custom_build_variant_cfg_flag \
  --disable-mock \
  --no-setup-mock \
  --no-checkout-sources \
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
