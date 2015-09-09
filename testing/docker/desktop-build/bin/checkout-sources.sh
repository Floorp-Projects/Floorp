#! /bin/bash -vex

set -x -e

# Inputs, with defaults

# mozharness builds use three repositories: gecko (source), mozharness (build
# scripts) and tools (miscellaneous) for each, specify *_REPOSITORY.  If the
# revision is not in the standard repo for the codebase, specify *_BASE_REPO as
# the canonical repo to clone and *_HEAD_REPO as the repo containing the
# desired revision.  For Mercurial clones, only *_HEAD_REV is required; for Git
# clones, specify the branch name to fetch as *_HEAD_REF and the desired sha1
# as *_HEAD_REV.

: GECKO_REPOSITORY              ${GECKO_REPOSITORY:=https://hg.mozilla.org/mozilla-central}
: GECKO_BASE_REPOSITORY         ${GECKO_BASE_REPOSITORY:=${GECKO_REPOSITORY}}
: GECKO_HEAD_REPOSITORY         ${GECKO_HEAD_REPOSITORY:=${GECKO_REPOSITORY}}
: GECKO_HEAD_REV                ${GECKO_HEAD_REV:=default}
: GECKO_HEAD_REF                ${GECKO_HEAD_REF:=${GECKO_HEAD_REV}}

: TOOLS_REPOSITORY              ${TOOLS_REPOSITORY:=https://hg.mozilla.org/build/tools}
: TOOLS_BASE_REPOSITORY         ${TOOLS_BASE_REPOSITORY:=${TOOLS_REPOSITORY}}
: TOOLS_HEAD_REPOSITORY         ${TOOLS_HEAD_REPOSITORY:=${TOOLS_REPOSITORY}}
: TOOLS_HEAD_REV                ${TOOLS_HEAD_REV:=default}
: TOOLS_HEAD_REF                ${TOOLS_HEAD_REF:=${TOOLS_HEAD_REV}}
: TOOLS_DISABLE                 ${TOOLS_DISABLE:=false}

: WORKSPACE                     ${WORKSPACE:=/home/worker/workspace}

set -v

# check out tools where mozharness expects it to be ($PWD/build/tools and $WORKSPACE/build/tools)
if [ ! "$TOOLS_DISABLE" = true ]
then
    tc-vcs checkout $WORKSPACE/build/tools $TOOLS_BASE_REPOSITORY $TOOLS_HEAD_REPOSITORY $TOOLS_HEAD_REV $TOOLS_HEAD_REF

    if [ ! -d build ]; then
        mkdir -p build
        ln -s $WORKSPACE/build/tools build/tools
    fi
fi

tc-vcs checkout $WORKSPACE/build/src $GECKO_BASE_REPOSITORY $GECKO_HEAD_REPOSITORY $GECKO_HEAD_REV $GECKO_HEAD_REF
