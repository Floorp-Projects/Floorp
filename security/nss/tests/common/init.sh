#! /bin/sh
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
#
########################################################################
#
# mozilla/security/nss/tests/common/init.sh
#
# initialization for NSS QA, can be included multiple times
# from all.sh and the individual scripts
#
# variables, utilities and shellfunctions global to NSS QA
# needs to work on all Unix and Windows platforms
#
# included from (don't expect this to be up to date)
# --------------------------------------------------
#   all.sh
#   ssl.sh
#   sdr.sh
#   cipher.sh
#   perf.sh
#   cert.sh
#   smime.sh
#   tools.sh
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
# NOTE:
# -----
#    Unlike the old QA this is based on files sourcing each other
#    This is done to save time, since a great portion of time is lost
#    in calling and sourcing the same things multiple times over the
#    network. Also, this way all scripts have all shell function  available
#    and a completely common environment
#
########################################################################


if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then

    Exit()
    {
        if [ -n "$1" ] ; then
            echo "$SCRIPTNAME: Exit: $*"
            html_failed "<TR><TD>$*"
        fi
        echo "</TABLE><BR>" >> ${RESULTS}
        if [ -n "${TAILPID}" ]; then
            ${KILL} "${TAILPID}"
        fi
        if [ -n "${SERVERPID}" -a -f "${SERVERPID}" ]; then
            ${KILL} `cat ${SERVERPID}`
        fi
        CLEANUP=${SCRIPTNAME}
        cd ${QADIR}
        . common/cleanup.sh
        case $1 in
            [0-4][0-9]|[0-9])
                exit $1;
                ;;
            *)
                exit 1
                ;;
        esac
    }

    html() #########################    write the results.html file
    {      # 3 functions so we can put targets in the output.log easier
        echo $* >>${RESULTS}
    }
    html_passed()
    {
        html "$* ${HTML_PASSED}"
    }
    html_failed()
    {
        html "$* ${HTML_FAILED}"
    }
    html_head()
    {
        html "<TABLE BORDER=1><TR><TH COLSPAN=3>$*</TH></TR>"
        html "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>" 
        echo "$SCRIPTNAME: $* ==============================="
    }
    html_msg()
    {
        if [ "$1" -ne "$2" ] ; then
            html_failed "<TR><TD>$3"
            if [ -n "$4" ] ; then
                echo "$SCRIPTNAME: $3 $4 FAILED"
            fi
        else
            html_passed "<TR><TD>$3"
            if [ -n "$4" ] ; then
                echo "$SCRIPTNAME: $3 $4 PASSED"
            fi
        fi
    }

    SCRIPTNAME=init.sh

    mozilla_root=`(cd ../../../..; pwd)`
    MOZILLA_ROOT=${MOZILLA_ROOT-$mozilla_root}

    qadir=`(cd ..; pwd)`
    QADIR=${QADIR-$qadir}

    common=${QADIR}/common
    COMMON=${TEST_COMMON-$common}
    export COMMON

    DIST=${DIST-${MOZILLA_ROOT}/dist}
    SECURITY_ROOT=${SECURITY_ROOT-${MOZILLA_ROOT}/security/nss}
    TESTDIR=${TESTDIR-${MOZILLA_ROOT}/tests_results/security}
    OBJDIR=`(cd $COMMON; gmake objdir_name)`
    OS_ARCH=`(cd $COMMON; gmake os_arch)`
    OS_NAME=`uname -s | sed -e "s/-[0-9]*\.[0-9]*//"`

    if [ -z "${DON_T_SET_PATHS}" -o "${DON_T_SET_PATHS}" != "TRUE" ] ; then
        if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME"  != "CYGWIN_NT" ]; then
            PATH=${DIST}/${OBJDIR}/bin\;${DIST}/${OBJDIR}/lib\;$PATH
            PATH=`perl ../path_uniq -d ';' "$PATH"`
        else
            PATH=${DIST}/${OBJDIR}/bin:${DIST}/${OBJDIR}/lib:$PATH
            PATH=`perl ../path_uniq -d ':' "$PATH"`
        fi

        LD_LIBRARY_PATH=${DIST}/${OBJDIR}/lib
        SHLIB_PATH=${DIST}/${OBJDIR}/lib
        LIBPATH=${DIST}/${OBJDIR}/lib
    fi

    if [ ! -d "${TESTDIR}" ]; then
        echo "$SCRIPTNAME init: Creating ${TESTDIR}"
        mkdir -p ${TESTDIR}
    fi

    case $HOST in
        *\.*)
            HOST=`echo $HOST | sed -e "s/\..*//"`
            ;;
        ?*)
            ;;
        *)
            echo "$SCRIPTNAME: Fatal HOST environment variable is not defined."
            exit 1 #does not need to be Exit, very early in script
            ;;
    esac

    if [ -z "${DOMSUF}" ]; then
        DOMSUF=`domainname`
        if  [ -z "${DOMSUF}" ]; then
            echo "$SCRIPTNAME: Fatal DOMSUF env. variable is not defined."
            exit 1 #does not need to be Exit, very early in script
        fi
    fi
    if [ -z "$USE_IP" -o "$USE_IP" != "TRUE" ] ; then
        HOSTADDR=${HOST}.${DOMSUF}
    else
        HOSTADDR=${IP_ADDRESS}
    fi

    #if running remote side of the distributed stress test we need to use the files that
    #the server side gives us...
    if [ -n "$DO_REM_ST" -a "$DO_REM_ST" = "TRUE" ] ; then
        for w in `ls -rtd ${TESTDIR}/${HOST}.[0-9]* 2>/dev/null |
            sed -e "s/.*${HOST}.//"` ; do
                version=$w
        done
        HOSTDIR=${TESTDIR}/${HOST}.$version
        echo "$SCRIPTNAME init: HOSTDIR $HOSTDIR"
        echo $HOSTDIR
        if [ ! -d $HOSTDIR ] ; then
            echo "$SCRIPTNAME: Fatal: Remote side of dist. stress test "
            echo "       - server HOSTDIR $HOSTDIR does not exist"
            exit 1 #does not need to be Exit, very early in script
        fi
    fi

    if [ -n "${HOSTDIR}" ]; then
        version=`echo $HOSTDIR | sed  -e "s/.*${HOST}.//"` 
    else
        if [ -f "${TESTDIR}/${HOST}" ]; then
            version=`cat ${TESTDIR}/${HOST}`
        else
            version=1
        fi
        if [ -z "${version}" ]; then    # for some strange reason this file
                                        # gets truncated at times... Windos
            for w in `ls -d ${TESTDIR}/${HOST}.[0-9]* 2>/dev/null |
                sort -t '.' -n | sed -e "s/.*${HOST}.//"` ; do
                version=`expr $w + 1`
            done
            if [ -z "${version}" ]; then
                version=1
            fi
        fi
        expr $version + 1 > ${TESTDIR}/${HOST}

        HOSTDIR=${TESTDIR}/${HOST}'.'$version

        mkdir -p ${HOSTDIR}
    fi

    if [ -z "${LOGFILE}" ]; then
        LOGFILE=${HOSTDIR}/output.log
    fi
    if [ ! -f "${LOGFILE}" ]; then
        touch ${LOGFILE}
    fi
    if [ -z "${RESULTS}" ]; then
        RESULTS=${HOSTDIR}/results.html
    fi
    if [ ! -f "${RESULTS}" ]; then
        cp ${COMMON}/results_header.html ${RESULTS}
        html "<H4>Platform: ${OBJDIR}<BR>" 
        html "Test Run: ${HOST}.$version</H4>" 
        html "${BC_ACTION}"
        html "<HR><BR>" 
        html "<HTML><BODY>" 

        echo "********************************************" | tee ${LOGFILE}
        echo "   Platform: ${OBJDIR}" | tee ${LOGFILE}
        echo "   Results: ${HOST}.$version" | tee ${LOGFILE}
        echo "********************************************" | tee ${LOGFILE}
	echo "$BC_ACTION" | tee ${LOGFILE}
    #if running remote side of the distributed stress test let the user know who it is...
    elif [ -n "$DO_REM_ST" -a "$DO_REM_ST" = "TRUE" ] ; then
        echo "********************************************" | tee ${LOGFILE}
        echo "   Platform: ${OBJDIR}" | tee ${LOGFILE}
        echo "   Results: ${HOST}.$version" | tee ${LOGFILE}
        echo "   remote side of distributed stress test " | tee ${LOGFILE}
        echo "   `uname -n -s`" | tee ${LOGFILE}
        echo "********************************************" | tee ${LOGFILE}
    fi

    echo "$SCRIPTNAME init: Testing PATH $PATH against LIB $LD_LIBRARY_PATH" |
        tee ${LOGFILE}

    KILL="kill"
    if  [ "${OS_ARCH}" = "Linux" ]; then
        SLEEP="sleep 30"
    fi
    if [ `uname -s` = "SunOS" ]; then
        PS="/usr/5bin/ps"
    else
        PS="ps"
    fi
    #found 3 rsh's so far that do not work as expected - cygnus mks6 (restricted sh) and mks 7
    if [ -z "$RSH" ]; then
        if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME"  = "CYGWIN_NT" ]; then
            RSH=/cygdrive/c/winnt/system32/rsh
        elif [ "${OS_ARCH}" = "WINNT" ]; then
            RSH=c:/winnt/system32/rsh
        else
            RSH=rsh
        fi
    fi
   

    CURDIR=`pwd`

    HTML_FAILED='</TD><TD bgcolor=red>Failed</TD><TR>'
    HTML_PASSED='</TD><TD bgcolor=lightGreen>Passed</TD><TR>'

    CU_ACTION='Unknown certutil action'

    # would like to preserve some tmp files, also easier to see if there 
    # are "leftovers" - another possibility ${HOSTDIR}/tmp

    TMP=${HOSTDIR}      #TMP=${TMP-/tmp}

    CADIR=${HOSTDIR}/CA
    SERVERDIR=${HOSTDIR}/server
    CLIENTDIR=${HOSTDIR}/client
    ALICEDIR=${HOSTDIR}/alicedir
    BOBDIR=${HOSTDIR}/bobdir
    DAVEDIR=${HOSTDIR}/dave

    PWFILE=${TMP}/tests.pw.$$
    NOISE_FILE=${TMP}/tests_noise.$$

    # we need relative pathnames of these files abd directories, since our 
    # tools can't handle the unix style absolut pathnames on cygnus

    R_CADIR=../CA
    R_SERVERDIR=../server
    R_CLIENTDIR=../client
    R_ALICEDIR=../alicedir
    R_BOBDIR=../bobdir
    R_DAVEDIR=../dave

    R_PWFILE=../tests.pw.$$
    R_NOISE_FILE=../tests_noise.$$

    # a new log file, short - fast to search, mostly for tools to
    # see if their portion of the cert has succeeded, also for me -
    CERT_LOG_FILE=${HOSTDIR}/cert.log      #the output.log is so crowded...

    TEMPFILES="${PWFILE} ${NOISE_FILE}"
    trap "Exit $0 Signal_caught" 2 3

    export PATH LD_LIBRARY_PATH SHLIB_PATH LIBPATH
    export DOMSUF HOSTADDR
    export KILL SLEEP PS
    export MOZILLA_ROOT SECURITY_ROOT DIST TESTDIR OBJDIR HOSTDIR QADIR
    export LOGFILE SCRIPTNAME

    if [ -z "$GLOB_MIN_CERT" ] ; then
        GLOB_MIN_CERT=0
    fi
    if [ -z "$GLOBMAX_CERT" ] ; then
        GLOB_MAX_CERT=200
    fi
    if [ -z "$MIN_CERT" ] ; then
        MIN_CERT=$GLOB_MIN_CERT
    fi
    if [ -z "$MAX_CERT" ] ; then
        MAX_CERT=$GLOB_MAX_CERT
    fi

    SCRIPTNAME=$0
    INIT_SOURCED=TRUE   #whatever one does - NEVER export this one please
fi
