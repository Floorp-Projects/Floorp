#!/bin/sh
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
    echo ${2}/shlibsign -v -i ${5}
    ${2}/shlibsign -v -i ${5}
    ;;
OpenVMS)
    temp="tmp$$.tmp"
    temp2="tmp$$.tmp2"
    cd ${1}/lib
    vmsdir=`dcl show default`
    ls *.so > $temp
    sed -e "s/\([^\.]*\)\.so/\$ define\/job \1 ${vmsdir}\1.so/" $temp > $temp2
    echo '$ define/job getipnodebyname xxx' >> $temp2
    echo '$ define/job vms_null_dl_name sys$share:decc$shr' >> $temp2
    dcl @$temp2
    echo ${2}/shlibsign -v -i ${5}
    ${2}/shlibsign -v -i ${5}
    sed -e "s/\([^\.]*\)\.so/\$ deass\/job \1/" $temp > $temp2
    echo '$ deass/job getipnodebyname' >> $temp2
    echo '$ deass/job vms_null_dl_name' >> $temp2
    dcl @$temp2
    rm $temp $temp2
    ;;
*)
    LIBPATH=`(cd ${1}/lib; pwd)`:`(cd ${4}; pwd)`:$LIBPATH
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
    echo ${2}/shlibsign -v -i ${5}
    ${2}/shlibsign -v -i ${5}
    ;;
esac
