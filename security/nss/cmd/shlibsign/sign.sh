#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# arguments:
# 1: full path to DIST/OBJDIR (parent dir of "lib")
# 2: full path to shlibsign executable (DIST/OBJDIR/bin)
# 3: OS_TARGET
# 4: full path to DIST/OBJDIR/lib
# 5: full path to library that is to be signed

case "${3}" in
WIN*)
    if echo "${PATH}" | grep -c \; >/dev/null; then
        PATH=${1}/lib\;${1}/bin\;${4}\;${PATH}
    else
        # ARG1 is ${1} with the drive letter escaped.
        if echo "${1}" | grep -c : >/dev/null; then
            ARG1=`(cd ${1}; pwd)`
        else
            ARG1=${1}
        fi
        if echo "${4}" | grep -c : >/dev/null; then
            ARG4=`(cd ${4}; pwd)`
        else
            ARG4=${4}
        fi
        PATH=${ARG1}/lib:${ARG1}/bin:${ARG4}:${PATH}
    fi
    export PATH
    echo "${2}"/shlibsign -v -i "${5}"
    "${2}"/shlibsign -v -i "${5}"
    ;;
*)
    LIBPATH=`(cd "${1}"/lib; pwd)`:`(cd "${4}"; pwd)`:$LIBPATH
    export LIBPATH
    SHLIB_PATH=${1}/lib:${4}:$SHLIB_PATH
    export SHLIB_PATH
    LD_LIBRARY_PATH=${1}/lib:${4}:$LD_LIBRARY_PATH
    export LD_LIBRARY_PATH
    DYLD_LIBRARY_PATH=${1}/lib:${4}:$DYLD_LIBRARY_PATH
    export DYLD_LIBRARY_PATH
    LIBRARY_PATH=${1}/lib:${4}:$LIBRARY_PATH
    export LIBRARY_PATH
    ADDON_PATH=${1}/lib:${4}:$ADDON_PATH
    export ADDON_PATH
    echo "${2}"/shlibsign -v -i "${5}"
    "${2}"/shlibsign -v -i "${5}"
    ;;
esac
