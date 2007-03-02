#! /bin/sh
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
# included from 
# -------------
#   all.sh
#   ssl.sh
#   sdr.sh
#   cipher.sh
#   perf.sh
#   cert.sh
#   smime.sh
#   tools.sh
#   fips.sh
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

NSS_STRICT_SHUTDOWN=1
export NSS_STRICT_SHUTDOWN

if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then

# Exit shellfunction to clean up at exit (error, regular or signal)
    Exit()
    {
        if [ -n "$1" ] ; then
            echo "$SCRIPTNAME: Exit: $*"
            html_failed "<TR><TD>$*"
        fi
        echo "</TABLE><BR>" >> ${RESULTS}
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

    detect_core()
    {
        [ ! -f $CORELIST_FILE ] && touch $CORELIST_FILE
        mv $CORELIST_FILE ${CORELIST_FILE}.old
        coreStr=`find $HOSTDIR -type f -name '*core*'`
        res=0
        if [ -n "$coreStr" ]; then
            sum $coreStr > $CORELIST_FILE
            res=`cat $CORELIST_FILE ${CORELIST_FILE}.old | sort | uniq -u | wc -l`
        fi
        return $res
    }

#html functions to give the resultfiles a consistant look
    html() #########################    write the results.html file
    {      # 3 functions so we can put targets in the output.log easier
        echo $* >>${RESULTS}
    }
    html_passed()
    {
        html_detect_core "$@" || return
        html "$* ${HTML_PASSED}"
    }
    html_failed()
    {
        html_detect_core "$@" || return
        html "$* ${HTML_FAILED}"
    }
    html_detect_core()
    {
        detect_core
        if [ $? -ne 0 ]; then
            echo "$*. Core file is detected."
            html "$* ${HTML_FAILED_CORE}"
            return 1
        fi
        return 0
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
    HTML_FAILED='</TD><TD bgcolor=red>Failed</TD><TR>'
    HTML_FAILED_CORE='</TD><TD bgcolor=red>Failed Core</TD><TR>'
    HTML_PASSED='</TD><TD bgcolor=lightGreen>Passed</TD><TR>'


#directory name init
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
    DLL_PREFIX=`(cd $COMMON; gmake dll_prefix)`
    DLL_SUFFIX=`(cd $COMMON; gmake dll_suffix)`
    OS_NAME=`uname -s | sed -e "s/-[0-9]*\.[0-9]*//"`

    # Pathnames constructed from ${TESTDIR} are passed to NSS tools
    # such as certutil, which don't understand Cygwin pathnames.
    # So we need to convert ${TESTDIR} to a Windows pathname (with
    # regular slashes).
    if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "CYGWIN_NT" ]; then
        TESTDIR=`cygpath -m ${TESTDIR}`
    fi

#in case of backward comp. tests the calling scripts set the
#PATH and LD_LIBRARY_PATH and do not want them to be changed
    if [ -z "${DON_T_SET_PATHS}" -o "${DON_T_SET_PATHS}" != "TRUE" ] ; then
        if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME"  != "CYGWIN_NT" ]; then
            PATH=.\;${DIST}/${OBJDIR}/bin\;${DIST}/${OBJDIR}/lib\;$PATH
            PATH=`perl ../path_uniq -d ';' "$PATH"`
        else
            PATH=.:${DIST}/${OBJDIR}/bin:${DIST}/${OBJDIR}/lib:/bin:/usr/bin:$PATH
            # added /bin and /usr/bin in the beginning so a local perl will 
            # be used
            PATH=`perl ../path_uniq -d ':' "$PATH"`
        fi

        LD_LIBRARY_PATH=${DIST}/${OBJDIR}/lib:$LD_LIBRARY_PATH
        SHLIB_PATH=${DIST}/${OBJDIR}/lib:$SHLIB_PATH
        LIBPATH=${DIST}/${OBJDIR}/lib:$LIBPATH
        DYLD_LIBRARY_PATH=${DIST}/${OBJDIR}/lib:$DYLD_LIBRARY_PATH
    fi

    if [ ! -d "${TESTDIR}" ]; then
        echo "$SCRIPTNAME init: Creating ${TESTDIR}"
        mkdir -p ${TESTDIR}
    fi

#HOST and DOMSUF are needed for the server cert 
    case $HOST in
        *\.*)
            HOST=`echo $HOST | sed -e "s/\..*//"`
            ;;
        ?*)
            ;;
        *)
            HOST=`uname -n`
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
            ;;
    esac

    if [ -z "${DOMSUF}" ]; then
        DOMSUF=`domainname`
        if  [ -z "${DOMSUF}" ]; then
            echo "$SCRIPTNAME: Fatal DOMSUF env. variable is not defined."
            exit 1 #does not need to be Exit, very early in script
        fi
    fi
#HOSTADDR was a workaround for the dist. stress test, and is probably 
#not needed anymore (purpose: be able to use IP address for the server 
#cert instead of PC name which was not in the DNS because of dyn IP address
    if [ -z "$USE_IP" -o "$USE_IP" != "TRUE" ] ; then
        HOSTADDR=${HOST}.${DOMSUF}
    else
        HOSTADDR=${IP_ADDRESS}
    fi

#if running remote side of the distributed stress test we need to use 
#the files that the server side gives us...
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

#find the HOSTDIR, where the results are supposed to go
    if [ -n "${HOSTDIR}" ]; then
        version=`echo $HOSTDIR | sed  -e "s/.*${HOST}.//"` 
    else
        if [ -f "${TESTDIR}/${HOST}" ]; then
            version=`cat ${TESTDIR}/${HOST}`
        else
            version=1
        fi
#file has a tendency to disappear, messing up the rest of QA - 
#workaround to find the next higher number if version file is not there
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

#result and log file and filename init,
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

    if [ `uname -s` = "SunOS" ]; then
        PS="/usr/5bin/ps"
    else
        PS="ps"
    fi
#found 3 rsh's so far that do not work as expected - cygnus mks6 
#(restricted sh) and mks 7 - if it is not in c:/winnt/system32 it
#needs to be set in the environ.ksh
    if [ -z "$RSH" ]; then
        if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME"  = "CYGWIN_NT" ]; then
            RSH=/cygdrive/c/winnt/system32/rsh
        elif [ "${OS_ARCH}" = "WINNT" ]; then
            RSH=c:/winnt/system32/rsh
        else
            RSH=rsh
        fi
    fi
   

#more filename and directoryname init
    CURDIR=`pwd`

    CU_ACTION='Unknown certutil action'

    # would like to preserve some tmp files, also easier to see if there 
    # are "leftovers" - another possibility ${HOSTDIR}/tmp

    TMP=${HOSTDIR}      #TMP=${TMP-/tmp}
    TEMP=${TMP}
    TMPDIR=${TMP}

    CADIR=${HOSTDIR}/CA
    SERVERDIR=${HOSTDIR}/server
    CLIENTDIR=${HOSTDIR}/client
    ALICEDIR=${HOSTDIR}/alicedir
    BOBDIR=${HOSTDIR}/bobdir
    DAVEDIR=${HOSTDIR}/dave
    EVEDIR=${HOSTDIR}/eve
    FIPSDIR=${HOSTDIR}/fips
    ECCURVES_DIR=${HOSTDIR}/eccurves

    SERVER_CADIR=${HOSTDIR}/serverCA
    CLIENT_CADIR=${HOSTDIR}/clientCA
    EXT_SERVERDIR=${HOSTDIR}/ext_server
    EXT_CLIENTDIR=${HOSTDIR}/ext_client

    IOPR_CADIR=${HOSTDIR}/CA_iopr
    IOPR_SSL_SERVERDIR=${HOSTDIR}/server_ssl_iopr
    IOPR_SSL_CLIENTDIR=${HOSTDIR}/client_ssl_iopr
    IOPR_OCSP_CLIENTDIR=${HOSTDIR}/client_ocsp_iopr

    CERT_EXTENSIONS_DIR=${HOSTDIR}/cert_extensions

    PWFILE=${TMP}/tests.pw.$$
    NOISE_FILE=${TMP}/tests_noise.$$
    CORELIST_FILE=${TMP}/clist.$$

    FIPSPWFILE=${TMP}/tests.fipspw.$$
    FIPSBADPWFILE=${TMP}/tests.fipsbadpw.$$
    FIPSP12PWFILE=${TMP}/tests.fipsp12pw.$$
    FIPSCERTNICK="FIPS_PUB_140_Test_Certificate"

    # domains to handle ipc based access to databases
    D_CA="TestCA.$version"
    D_ALICE="Alice.$version"
    D_BOB="Bob.$version"
    D_DAVE="Dave.$version"
    D_EVE="Eve.$version"
    D_SERVER_CA="ServerCA.$version"
    D_CLIENT_CA="ClientCA.$version"
    D_SERVER="Server.$version"
    D_CLIENT="Client.$version"
    D_FIPS="FIPS.$version"
    D_ECCURVES="ECCURVES.$version"
    D_EXT_SERVER="ExtendedServer.$version"
    D_EXT_CLIENT="ExtendedClient.$version"
    D_CERT_EXTENSTIONS="CertExtensions.$version"

    # we need relative pathnames of these files abd directories, since our 
    # tools can't handle the unix style absolut pathnames on cygnus

    R_CADIR=../CA
    R_SERVERDIR=../server
    R_CLIENTDIR=../client
    R_IOPR_CADIR=../CA_iopr
    R_IOPR_SSL_SERVERDIR=../server_ssl_iopr
    R_IOPR_SSL_CLIENTDIR=../client_ssl_iopr
    R_IOPR_OCSP_CLIENTDIR=../client_ocsp_iopr
    R_ALICEDIR=../alicedir
    R_BOBDIR=../bobdir
    R_DAVEDIR=../dave
    R_EVEDIR=../eve
    R_EXT_SERVERDIR=../ext_server
    R_EXT_CLIENTDIR=../ext_client
    R_CERT_EXT=../cert_extensions

    #
    # profiles are either paths or domains depending on the setting of
    # MULTIACCESS_DBM
    #
    P_R_CADIR=${R_CADIR}
    P_R_ALICEDIR=${R_ALICEDIR}
    P_R_BOBDIR=${R_BOBDIR}
    P_R_DAVEDIR=${R_DAVEDIR}
    P_R_EVEDIR=${R_EVEDIR}
    P_R_SERVERDIR=${R_SERVERDIR}
    P_R_CLIENTDIR=${R_CLIENTDIR}
    P_R_EXT_SERVERDIR=${R_EXT_SERVERDIR}
    P_R_EXT_CLIENTDIR=${R_EXT_CLIENTDIR}
    P_SERVER_CADIR=${SERVER_CADIR}
    P_CLIENT_CADIR=${CLIENT_CADIR}
    if [ -n "${MULTIACCESS_DBM}" ]; then
	P_R_CADIR="multiaccess:${D_CA}"
	P_R_ALICEDIR="multiaccess:${D_ALICE}"
	P_R_BOBDIR="multiaccess:${D_BOB}"
	P_R_DAVEDIR="multiaccess:${D_DAVE}"
	P_R_EVEDIR="multiaccess:${D_EVE}"
	P_R_SERVERDIR="multiaccess:${D_SERVER}"
	P_R_CLIENTDIR="multiaccess:${D_CLIENT}"
	P_R_EXT_SERVERDIR="multiaccess:${D_EXT_SERVER}"
	P_R_EXT_CLIENTDIR="multiaccess:${D_EXT_CLIENT}"
	P_SERVER_CADIR="multiaccess:${D_SERVER_CA}"
	P_CLIENT_CADIR="multiaccess:${D_CLIENT_CA}"
    fi

    R_PWFILE=../tests.pw.$$
    R_NOISE_FILE=../tests_noise.$$

    R_FIPSPWFILE=../tests.fipspw.$$
    R_FIPSBADPWFILE=../tests.fipsbadpw.$$
    R_FIPSP12PWFILE=../tests.fipsp12pw.$$

    echo "fIps140" > ${FIPSPWFILE}
    echo "fips104" > ${FIPSBADPWFILE}
    echo "pKcs12fips140" > ${FIPSP12PWFILE}

    # a new log file, short - fast to search, mostly for tools to
    # see if their portion of the cert has succeeded, also for me -
    CERT_LOG_FILE=${HOSTDIR}/cert.log      #the output.log is so crowded...

    TEMPFILES="${PWFILE} ${NOISE_FILE}"
    trap "Exit $0 Signal_caught" 2 3

    export PATH LD_LIBRARY_PATH SHLIB_PATH LIBPATH DYLD_LIBRARY_PATH
    export DOMSUF HOSTADDR
    export KILL PS
    export MOZILLA_ROOT SECURITY_ROOT DIST TESTDIR OBJDIR HOSTDIR QADIR
    export LOGFILE SCRIPTNAME

#used for the distributed stress test, the server generates certificates 
#from GLOB_MIN_CERT to GLOB_MAX_CERT 
# NOTE - this variable actually gets initialized by directly by the 
# ssl_dist_stress.shs sl_ds_init() before init is called - need to change 
# in  both places. speaking of data encapsulatioN...

    if [ -z "$GLOB_MIN_CERT" ] ; then
        GLOB_MIN_CERT=0
    fi
    if [ -z "$GLOB_MAX_CERT" ] ; then
        GLOB_MAX_CERT=200
    fi
    if [ -z "$MIN_CERT" ] ; then
        MIN_CERT=$GLOB_MIN_CERT
    fi
    if [ -z "$MAX_CERT" ] ; then
        MAX_CERT=$GLOB_MAX_CERT
    fi

    #################################################
    # CRL SSL testing constatnts
    #


    CRL_GRP_1_BEGIN=40
    CRL_GRP_1_RANGE=3
    UNREVOKED_CERT_GRP_1=41

    CRL_GRP_2_BEGIN=43
    CRL_GRP_2_RANGE=6
    UNREVOKED_CERT_GRP_2=46

    CRL_GRP_3_BEGIN=49
    CRL_GRP_3_RANGE=4
    UNREVOKED_CERT_GRP_3=51

    TOTAL_CRL_RANGE=`expr ${CRL_GRP_1_RANGE} + ${CRL_GRP_2_RANGE} + \
                     ${CRL_GRP_3_RANGE}`

    TOTAL_GRP_NUM=3
    
    RELOAD_CRL=1

    #################################################
    # Interoperability testing constatnts
    #
    # if suite is setup for testing, IOPR_HOSTADDR_LIST should have
    # at least one host name(FQDN)
    # Example   IOPR_HOSTADDR_LIST="goa1.SFBay.Sun.COM"

    if [ -z "`echo ${IOPR_HOSTADDR_LIST} | grep '[A-Za-z]'`" ]; then
        IOPR=0
    else
        IOPR=1
    fi
    #################################################

    SCRIPTNAME=$0
    INIT_SOURCED=TRUE   #whatever one does - NEVER export this one please
fi
