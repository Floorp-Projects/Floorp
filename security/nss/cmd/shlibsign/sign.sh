#!/bin/sh
if [ ${3} = "YES" ]; then
    if test `echo "${PATH}" | grep -c \;` != 0; then
        PATH=${PATH}\;${1}/bin\;${1}/lib
    else
        PATH=${PATH}:${1}/bin:${1}/lib
    fi
    export PATH
else
    LIBPATH=${1}/lib
    export LIBPATH
    SHLIB_PATH=${1}/lib
    export SHLIB_PATH
    LD_LIBRARY_PATH=${1}/lib
    export LD_LIBRARY_PATH
    DYLD_LIBRARY_PATH=${1}/lib
    export DYLD_LIBRARY_PATH
fi
echo ./${2}/shlibsign -v -i ${4}
./${2}/shlibsign -v -i ${4}
