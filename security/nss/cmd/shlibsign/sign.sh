#!/bin/sh
if [ ${3} = "YES" ]; then
    if echo "${PATH}" | grep -c \; >/dev/null; then
        PATH=${PATH}\;${1}/bin\;${1}/lib
    else
        # ARG1 is ${1} with the drive letter escaped.
        if echo "${1}" | grep -c : >/dev/null; then
            ARG1=`(cd ${1}; pwd)`
        else
            ARG1=${1}
        fi
        PATH=${PATH}:${ARG1}/bin:${ARG1}/lib
    fi
    export PATH
else
    LIBPATH=`(cd ${1}/lib; pwd)`
    export LIBPATH
    SHLIB_PATH=${1}/lib
    export SHLIB_PATH
    LD_LIBRARY_PATH=${1}/lib
    export LD_LIBRARY_PATH
    DYLD_LIBRARY_PATH=${1}/lib
    export DYLD_LIBRARY_PATH
    LIBRARY_PATH=${1}/lib:$LIBRARY_PATH
    export LIBRARY_PATH
fi
echo ${2}/shlibsign -v -i ${4}
${2}/shlibsign -v -i ${4}
