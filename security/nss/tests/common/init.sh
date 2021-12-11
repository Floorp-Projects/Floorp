#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

# Init directories based on HOSTDIR variable
if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
    init_directories()
    {
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
        DBPASSDIR=${HOSTDIR}/dbpass
        ECCURVES_DIR=${HOSTDIR}/eccurves
        DISTRUSTDIR=${HOSTDIR}/distrust
        RSAPSSDIR=${HOSTDIR}/rsapss

        SERVER_CADIR=${HOSTDIR}/serverCA
        CLIENT_CADIR=${HOSTDIR}/clientCA
        EXT_SERVERDIR=${HOSTDIR}/ext_server
        EXT_CLIENTDIR=${HOSTDIR}/ext_client
        IMPLICIT_INIT_DIR=${HOSTDIR}/implicit_init

        IOPR_CADIR=${HOSTDIR}/CA_iopr
        IOPR_SSL_SERVERDIR=${HOSTDIR}/server_ssl_iopr
        IOPR_SSL_CLIENTDIR=${HOSTDIR}/client_ssl_iopr
        IOPR_OCSP_CLIENTDIR=${HOSTDIR}/client_ocsp_iopr

        CERT_EXTENSIONS_DIR=${HOSTDIR}/cert_extensions
        STAPLINGDIR=${HOSTDIR}/stapling
        NOLOGINDIR=${HOSTDIR}/nologin
        SSLGTESTDIR=${HOSTDIR}/ssl_gtests
        GTESTDIR=${HOSTDIR}/gtests

        PWFILE=${HOSTDIR}/tests.pw
        EMPTY_FILE=${HOSTDIR}/tests_empty
        NOISE_FILE=${HOSTDIR}/tests_noise
        CORELIST_FILE=${HOSTDIR}/clist

        FIPSPWFILE=${HOSTDIR}/tests.fipspw
        FIPSBADPWFILE=${HOSTDIR}/tests.fipsbadpw
        FIPSP12PWFILE=${HOSTDIR}/tests.fipsp12pw

        echo nss > ${PWFILE}
        echo > ${EMPTY_FILE}
        echo "fIps140" > ${FIPSPWFILE}
        echo "fips104" > ${FIPSBADPWFILE}
        echo "pKcs12fips140" > ${FIPSP12PWFILE}

        noise

        P_SERVER_CADIR=${SERVER_CADIR}
        P_CLIENT_CADIR=${CLIENT_CADIR}

        if [ -n "${MULTIACCESS_DBM}" ]; then
            P_SERVER_CADIR="multiaccess:${D_SERVER_CA}"
            P_CLIENT_CADIR="multiaccess:${D_CLIENT_CA}"
        fi


        # a new log file, short - fast to search, mostly for tools to
        # see if their portion of the cert has succeeded, also for me -
        CERT_LOG_FILE=${HOSTDIR}/cert.log      #the output.log is so crowded...

        TEMPFILES=foobar   # keep "${PWFILE} ${NOISE_FILE}" around

        export HOSTDIR
    }

# Generate noise file
    noise()
    {
        # NOTE: these keys are only suitable for testing, as this whole thing
        # bypasses the entropy gathering. Don't use this method to generate
        # keys and certs for product use or deployment.
        ps -efl > ${NOISE_FILE} 2>&1
        ps aux >> ${NOISE_FILE} 2>&1
        date >> ${NOISE_FILE} 2>&1
    }

# Print selected environment variable (used for backup)
    env_backup()
    {
        echo "HOSTDIR=\"${HOSTDIR}\""
        echo "TABLE_ARGS="
        echo "NSS_TEST_DISABLE_CRL=${NSS_TEST_DISABLE_CRL}"
        echo "NSS_SSL_TESTS=\"${NSS_SSL_TESTS}\""
        echo "NSS_SSL_RUN=\"${NSS_SSL_RUN}\""
        echo "NSS_DEFAULT_DB_TYPE=${NSS_DEFAULT_DB_TYPE}"
        echo "export NSS_DEFAULT_DB_TYPE"
        echo "NSS_ENABLE_PKIX_VERIFY=${NSS_ENABLE_PKIX_VERIFY}"
        echo "export NSS_ENABLE_PKIX_VERIFY"
        echo "init_directories"
    }

# Exit shellfunction to clean up at exit (error, regular or signal)
    Exit()
    {
        if [ -n "$1" ] ; then
            echo "$SCRIPTNAME: Exit: $* - FAILED"
            html_failed "$*"
        fi
        echo "</TABLE><BR>" >> ${RESULTS}
        if [ -n "${SERVERPID}" -a -f "${SERVERPID}" ]; then
            ${KILL} `cat ${SERVERPID}`
        fi
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
    increase_msg_id()
    {
        MSG_ID=$(( ${MSG_ID} + 1 ))
    }
    html_passed_ignore_core()
    {
        increase_msg_id
        html "<TR><TD>#${MSG_ID}: $1 ${HTML_PASSED}"
        echo "${SCRIPTNAME}: #${MSG_ID}: $* - PASSED"
    }
    html_passed()
    {
        html_detect_core "$@" || return
        html_passed_ignore_core "$@"
    }
    html_failed_ignore_core()
    {
        increase_msg_id
        html "<TR><TD>#${MSG_ID}: $1 ${HTML_FAILED}"
        echo "${SCRIPTNAME}: #${MSG_ID}: $* - FAILED"
    }
    html_failed()
    {
        html_detect_core "$@" || return
        html_failed_ignore_core "$@" || return
    }
    html_unknown_ignore_core()
    {
        increase_msg_id
        html "<TR><TD>#${MSG_ID}: $1 ${HTML_UNKNOWN}"
        echo "${SCRIPTNAME}: #${MSG_ID}: $* - UNKNOWN"
    }
    html_unknown()
    {
        html_detect_core "$@" || return
        increase_msg_id
        html "<TR><TD>#${MSG_ID}: $1 ${HTML_UNKNOWN}"
        echo "${SCRIPTNAME}: #${MSG_ID}: $* - UNKNOWN"
    }
    html_detect_core()
    {
        detect_core
        if [ $? -ne 0 ]; then
            increase_msg_id
            html "<TR><TD>#${MSG_ID}: $* ${HTML_FAILED_CORE}"
            echo "${SCRIPTNAME}: #${MSG_ID}: $* - Core file is detected - FAILED"
            return 1
        fi
        return 0
    }
    html_head()
    {

        html "<TABLE BORDER=1 ${TABLE_ARGS}><TR><TH COLSPAN=3>$*</TH></TR>"
        html "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>"
        echo "$SCRIPTNAME: $* ==============================="
    }
    html_msg()
    {
        if [ $1 -ne $2 ] ; then
            html_failed "$3" "$4"
        else
            html_passed "$3" "$4"
        fi
    }
    HTML_FAILED='</TD><TD bgcolor=red>Failed</TD><TR>'
    HTML_FAILED_CORE='</TD><TD bgcolor=red>Failed Core</TD><TR>'
    HTML_PASSED='</TD><TD bgcolor=lightGreen>Passed</TD><TR>'
    HTML_UNKNOWN='</TD><TD>Unknown</TD><TR>'
    TABLE_ARGS=

    gtest_parse_report_helper()
    {
      # Check XML reports for normal test runs and failures.
      local successes=$(gtest_parse_report_xpath "//testcase[@status='run'][count(*)=0]" "$@" )
      local failures=$(gtest_parse_report_xpath "//failure/.." "$@" )

      # Print all tests that succeeded.
      while read result name; do
        html_passed_ignore_core "$name"
      done <<< "$successes"

      # Print failing tests.
      if [ -n "$failures" ]; then
        printf "\nFAILURES:\n=========\n"

        while read result name; do
          html_failed_ignore_core "$name"
        done <<< "$failures"

        printf "\n"
      fi
    }

    # This legacy report parser can't actually detect failures. It always relied
    # on the binary's exit code. Print the tests we ran to keep the old behavior.
    gtest_parse_report_legacy()
    {
      while read result name && [ -n "$name" ]; do
        if [ "$result" = "notrun" ]; then
          echo "$name" SKIPPED
        elif [ "$result" = "run" ]; then
          html_passed_ignore_core "$name"
        else
          html_failed_ignore_core "$name"
        fi
      done <<< "$(sed -f "${COMMON}/parsegtestreport.sed" "$@" )"
      # here's how we would use bash if it wasn't so slow
      # done <<< "$(sh "${COMMON}/parsegtestreport.sh" "$@" )"
    }

    gtest_parse_report_xpath()
    {
      # Query the XML report with the given XPath pattern.
      xpath="$1"
      shift
      xmllint --xpath "${xpath}" "$@" 2>/dev/null | \
        # Insert newlines to help sed.
        sed $'s/<testcase/\\\n<testcase/g' | \
        # Use sed to parse the report.
        sed -f "${COMMON}/parsegtestreport.sed"
        # here's how we would use bash if it wasn't so slow
        #sh "${COMMON}/parsegtestreport.sh"
    }

    gtest_parse_report()
    {
      if type xmllint &>/dev/null; then
        echo "DEBUG: Using xmllint to parse GTest XML report(s)"
        gtest_parse_report_helper "$@"
      else
        echo "DEBUG: Falling back to legacy XML report parsing using only sed"
        gtest_parse_report_legacy "$@"
      fi
    }



#directory name init
    SCRIPTNAME=init.sh

    mozilla_root=`(cd ../../..; pwd)`
    MOZILLA_ROOT=${MOZILLA_ROOT-$mozilla_root}

    qadir=`(cd ..; pwd)`
    QADIR=${QADIR-$qadir}

    common=${QADIR}/common
    COMMON=${TEST_COMMON-$common}
    export COMMON

    DIST=${DIST-${MOZILLA_ROOT}/dist}
    TESTDIR=${TESTDIR-${MOZILLA_ROOT}/tests_results/security}

    # Allow for override options from a config file
    if [ -n "${OBJDIR}" -a -f ${DIST}/${OBJDIR}/platform.cfg ]; then
        . ${DIST}/${OBJDIR}/platform.cfg
    fi

    # only need make if we don't already have certain variables set
    if [ -z "${OBJDIR}" -o -z "${OS_ARCH}" -o -z "${DLL_PREFIX}" -o -z "${DLL_SUFFIX}" ]; then
        MAKE=gmake
        $MAKE -v >/dev/null 2>&1 || MAKE=make
        $MAKE -v >/dev/null 2>&1 || { echo "You are missing make."; exit 5; }
        MAKE="$MAKE --no-print-directory"
    fi

    if [ "${OBJDIR}" = "" ]; then
        if [ -f ${DIST}/latest ]; then
            OBJDIR=$(cat ${DIST}/latest)
        else
            OBJDIR=`($MAKE -s -C $COMMON objdir_name)`
        fi
    fi
    if [ "${OS_ARCH}" = "" ]; then
        OS_ARCH=`(cd $COMMON; $MAKE os_arch)`
    fi
    if [ "${DLL_PREFIX}" = "" ]; then
        DLL_PREFIX=`(cd $COMMON; $MAKE dll_prefix)`
    fi
    if [ "${DLL_SUFFIX}" = "" ]; then
        DLL_SUFFIX=`(cd $COMMON; $MAKE dll_suffix)`
    fi
    OS_NAME=`uname -s | sed -e "s/-[0-9]*\.[0-9]*//" | sed -e "s/-WOW64//"`

    BINDIR="${DIST}/${OBJDIR}/bin"

    # Pathnames constructed from ${TESTDIR} are passed to NSS tools
    # such as certutil, which don't understand Cygwin pathnames.
    # So we need to convert ${TESTDIR} to a Windows pathname (with
    # regular slashes).
    if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "CYGWIN_NT" ]; then
        TESTDIR=`cygpath -m ${TESTDIR}`
        QADIR=`cygpath -m ${QADIR}`
    fi

    # Same problem with MSYS/Mingw, except we need to start over with pwd -W
    if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "MINGW32_NT" ]; then
                mingw_mozilla_root=`(cd ../../..; pwd -W)`
                MINGW_MOZILLA_ROOT=${MINGW_MOZILLA_ROOT-$mingw_mozilla_root}
                TESTDIR=${MINGW_TESTDIR-${MINGW_MOZILLA_ROOT}/tests_results/security}
    fi

    # Same problem with MSYS/Mingw, except we need to start over with pwd -W
    if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME" = "MINGW32_NT" ]; then
                mingw_mozilla_root=`(cd ../../..; pwd -W)`
                MINGW_MOZILLA_ROOT=${MINGW_MOZILLA_ROOT-$mingw_mozilla_root}
                TESTDIR=${MINGW_TESTDIR-${MINGW_MOZILLA_ROOT}/tests_results/security}
    fi
    echo testdir is $TESTDIR

#in case of backward comp. tests the calling scripts set the
#PATH and LD_LIBRARY_PATH and do not want them to be changed
    if [ -z "${DON_T_SET_PATHS}" -o "${DON_T_SET_PATHS}" != "TRUE" ] ; then
        if [ "${OS_ARCH}" = "WINNT" -a "$OS_NAME"  != "CYGWIN_NT" -a "$OS_NAME" != "MINGW32_NT" ]; then
            PATH=.\;${DIST}/${OBJDIR}/bin\;${DIST}/${OBJDIR}/lib\;$PATH
            PATH=`perl ../path_uniq -d ';' "$PATH"`
        elif [ "${OS_ARCH}" = "Android" ]; then
            # android doesn't have perl, skip the uniq step
            PATH=.:${DIST}/${OBJDIR}/bin:${DIST}/${OBJDIR}/lib:$PATH
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

    if [ -z "$DOMSUF" ] && hash domainname 2>/dev/null; then
        DOMSUF=`domainname`
    fi
    # hostname -d and domainname both return (none) if hostname doesn't
    # include a dot.  Pretend we didn't get an answer.
    if [ "$DOMSUF" = "(none)" ]; then
        DOMSUF=
    fi

    if [ -z "$HOST" ]; then
        HOST=`uname -n`
    fi
    case "$HOST" in
        *\.*)
            if [ -z "$DOMSUF" ]; then
                DOMSUF="${HOST#*.}"
            fi
            HOST="${HOST%%.*}"
            ;;
        ?*)
            ;;
        *)
            echo "$SCRIPTNAME: Fatal HOST environment variable is not defined."
            exit 1 #does not need to be Exit, very early in script
            ;;
    esac

    if [ -z "$DOMSUF" -a "$OS_ARCH" != "Android" ]; then
        echo "$SCRIPTNAME: Fatal DOMSUF env. variable is not defined."
        exit 1 #does not need to be Exit, very early in script
    fi

#HOSTADDR was a workaround for the dist. stress test, and is probably
#not needed anymore (purpose: be able to use IP address for the server
#cert instead of PC name which was not in the DNS because of dyn IP address
    if [ "$USE_IP" != "TRUE" ] ; then
        if [ -z "$DOMSUF" ]; then
            HOSTADDR=${HOST}
        else
            HOSTADDR=${HOST}.${DOMSUF}
        fi
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

        echo "********************************************" | tee -a ${LOGFILE}
        echo "   Platform: ${OBJDIR}"                       | tee -a ${LOGFILE}
        echo "   Results: ${HOST}.$version"                 | tee -a ${LOGFILE}
        echo "********************************************" | tee -a ${LOGFILE}
        echo "$BC_ACTION"                                   | tee -a ${LOGFILE}
#if running remote side of the distributed stress test
# let the user know who it is...
    elif [ -n "$DO_REM_ST" -a "$DO_REM_ST" = "TRUE" ] ; then
        echo "********************************************" | tee -a ${LOGFILE}
        echo "   Platform: ${OBJDIR}"                       | tee -a ${LOGFILE}
        echo "   Results: ${HOST}.$version"                 | tee -a ${LOGFILE}
        echo "   remote side of distributed stress test "   | tee -a ${LOGFILE}
        echo "   `uname -n -s`"                             | tee -a ${LOGFILE}
        echo "********************************************" | tee -a ${LOGFILE}
    fi

    echo "$SCRIPTNAME init: Testing PATH $PATH against LIB $LD_LIBRARY_PATH" |\
        tee -a ${LOGFILE}

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

    init_directories

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
    D_NOLOGIN="NoLogin.$version"
    D_FIPS="FIPS.$version"
    D_DBPASS="DBPASS.$version"
    D_ECCURVES="ECCURVES.$version"
    D_EXT_SERVER="ExtendedServer.$version"
    D_EXT_CLIENT="ExtendedClient.$version"
    D_IMPLICIT_INIT="ImplicitInit.$version"
    D_CERT_EXTENSTIONS="CertExtensions.$version"
    D_DISTRUST="Distrust.$version"
    D_RSAPSS="RSAPSS.$version"

    # we need relative pathnames of these files and directories, since our
    # tools can't handle the unix style absolute pathnames on cygnus

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
    R_IMPLICIT_INIT_DIR=../implicit_init
    R_CERT_EXT=../cert_extensions
    R_STAPLINGDIR=../stapling
    R_NOLOGINDIR=../nologin
    R_SSLGTESTDIR=../ssl_gtests
    R_GTESTDIR=../gtests
    R_RSAPSSDIR=../rsapss

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
    P_R_NOLOGINDIR=${R_NOLOGINDIR}
    P_R_EXT_SERVERDIR=${R_EXT_SERVERDIR}
    P_R_EXT_CLIENTDIR=${R_EXT_CLIENTDIR}
    P_R_IMPLICIT_INIT_DIR=${R_IMPLICIT_INIT_DIR}
    P_R_RSAPSSDIR=${R_RSAPSSDIR}
    if [ -n "${MULTIACCESS_DBM}" ]; then
        P_R_CADIR="multiaccess:${D_CA}"
        P_R_ALICEDIR="multiaccess:${D_ALICE}"
        P_R_BOBDIR="multiaccess:${D_BOB}"
        P_R_DAVEDIR="multiaccess:${D_DAVE}"
        P_R_EVEDIR="multiaccess:${D_EVE}"
        P_R_SERVERDIR="multiaccess:${D_SERVER}"
        P_R_CLIENTDIR="multiaccess:${D_CLIENT}"
        P_R_NOLOGINDIR="multiaccess:${D_NOLOGIN}"
        P_R_EXT_SERVERDIR="multiaccess:${D_EXT_SERVER}"
        P_R_EXT_CLIENTDIR="multiaccess:${D_EXT_CLIENT}"
        P_R_IMPLICIT_INIT_DIR="multiaccess:${D_IMPLICIT_INIT}"
        P_R_RSAPSSDIR="multiaccess:${D_RSAPSS}"
    fi

    R_PWFILE=../tests.pw
    R_EMPTY_FILE=../tests_empty
    R_NOISE_FILE=../tests_noise

    R_FIPSPWFILE=../tests.fipspw
    R_FIPSBADPWFILE=../tests.fipsbadpw
    R_FIPSP12PWFILE=../tests.fipsp12pw

    trap "Exit $0 Signal_caught" 2 3

    export PATH LD_LIBRARY_PATH SHLIB_PATH LIBPATH DYLD_LIBRARY_PATH
    export DOMSUF HOSTADDR
    export KILL PS
    export MOZILLA_ROOT DIST TESTDIR OBJDIR QADIR
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

    # if test mode isn't set, test scripts default to expecting sql
    if [ "${TEST_MODE}" = "" ]; then
        NSS_DEFAULT_DB_TYPE=${NSS_DEFAULT_DB_TYPE:-"sql"}
        export NSS_DEFAULT_DB_TYPE
    fi

    MSG_ID=0

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

    if [ "${OS_ARCH}" != "WINNT" -a "${OS_ARCH}" != "Android" ]; then
        ulimit -c unlimited
    fi

    SCRIPTNAME=$0
    INIT_SOURCED=TRUE   #whatever one does - NEVER export this one please
fi
