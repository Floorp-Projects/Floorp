#! /bin/bash -vex

set -x -e

# Inputs, with defaults

: GECKO_HEAD_REPOSITORY              ${GECKO_HEAD_REPOSITORY:=https://hg.mozilla.org/mozilla-central}
: GECKO_HEAD_REV                ${GECKO_HEAD_REV:=default}

: SCRIPT_DOWNLOAD_PATH          ${SCRIPT_DOWNLOAD_PATH:=$PWD}
: SCRIPT_PATH                   ${SCRIPT_PATH:?"script path must be set"}
set -v

# download script from the gecko repository
url=${GECKO_HEAD_REPOSITORY}/raw-file/${GECKO_HEAD_REV}/${SCRIPT_PATH}
wget --directory-prefix=${SCRIPT_DOWNLOAD_PATH} $url
chmod +x `basename ${SCRIPT_PATH}`
