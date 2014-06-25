#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/all.sh
#
# Script to start selected available NSS QA suites on one machine
# this script is called or sourced by NSS QA which runs on all required 
# platforms
#
# Needs to work on all Unix and Windows platforms
#
# Currently available NSS QA suites:
# ----------------------------------
#   cipher.sh    - tests NSS ciphers
#   libpkix.sh   - tests PKIX functionality
#   cert.sh      - exercises certutil and creates certs necessary for 
#                  all other tests
#   dbtests.sh   - tests related to certificate databases
#   tools.sh     - tests the majority of the NSS tools
#   fips.sh      - tests basic functionallity of NSS in FIPS-compliant 
#                - mode
#   sdr.sh       - tests NSS SDR
#   crmf.sh      - CRMF/CMMF testing
#   smime.sh     - S/MIME testing
#   ssl.sh       - tests SSL V2 SSL V3 and TLS
#   ocsp.sh      - OCSP testing
#   merge.sh     - tests merging old and new shareable databases
#   pkits.sh     - NIST/PKITS tests 
#   chains.sh    - PKIX cert chains tests 
#   dbupgrade.sh - upgrade databases to new shareable version (used
#                  only in upgrade test cycle)
#   memleak.sh   - memory leak testing (optional)
#
# NSS testing is now devided to 4 cycles:
# ---------------------------------------
#   standard     - run test suites with defaults settings
#   pkix         - run test suites with PKIX enabled
#   upgradedb    - upgrade existing certificate databases to shareable 
#                  format (creates them if doesn't exist yet) and run 
#                  test suites with those databases
#   sharedb      - run test suites with shareable database format 
#                  enabled (databases are created directly to this 
#                  format)
#
# Mandatory environment variables (to be set before testing):
# -----------------------------------------------------------
#   HOST         - test machine host name
#   DOMSUF       - test machine domain name
#
# Optional environment variables to specify build to use:
# -------------------------------------------------------
#   BUILT_OPT    - use optimized/debug build 
#   USE_64       - use 64bit/32bit build
#
# Optional environment variables to enable specific NSS features:
# ---------------------------------------------------------------
#   NSS_DISABLE_ECC             - disable ECC
#   NSS_ECC_MORE_THAN_SUITE_B   - enable extended ECC
#
# Optional environment variables to select which cycles/suites to test:
# ---------------------------------------------------------------------
#   NSS_CYCLES     - list of cycles to run (separated by space 
#                    character)
#                  - by default all cycles are tested
#
#   NSS_TESTS      - list of all test suites to run (separated by space
#                    character, without trailing .sh) 
#                  - this list can be reduced for individual test cycles
#
#   NSS_SSL_TESTS  - list of ssl tests to run (see ssl.sh)
#   NSS_SSL_RUN    - list of ssl sub-tests to run (see ssl.sh)
#
# Testing schema:
# ---------------
#                           all.sh                       ~  (main) 
#                              |                               |
#          +------------+------------+-----------+       ~  run_cycles
#          |            |            |           |             |
#      standard       pkix       upgradedb     sharedb   ~  run_cycle_*
#                       |                                      |
#                +------+------+------+----->            ~  run_tests
#                |      |      |      |                        |
#              cert   tools   fips   ssl   ...           ~  . *.sh
#
# Special strings:
# ----------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
# NOTE:
# -----
#   Unlike the old QA this is based on files sourcing each other
#   This is done to save time, since a great portion of time is lost
#   in calling and sourcing the same things multiple times over the
#   network. Also, this way all scripts have all shell function 
#   available and a completely common environment
#
########################################################################

############################## run_tests ###############################
# run test suites defined in TESTS variable, skip scripts defined in
# TESTS_SKIP variable
########################################################################
run_tests()
{
    for TEST in ${TESTS}
    do
        echo "${TESTS_SKIP}" | grep "${TEST}" > /dev/null
        if [ $? -eq 0 ]; then
            continue
        fi

        SCRIPTNAME=${TEST}.sh
        echo "Running tests for ${TEST}"
        echo "TIMESTAMP ${TEST} BEGIN: `date`" 
        (cd ${QADIR}/${TEST}; . ./${SCRIPTNAME} 2>&1)
        echo "TIMESTAMP ${TEST} END: `date`"
    done
}

########################## run_cycle_standard ##########################
# run test suites with defaults settings (no PKIX, no sharedb)
########################################################################
run_cycle_standard()
{
    TEST_MODE=STANDARD

    TESTS="${ALL_TESTS}"
    TESTS_SKIP=

    run_tests
}

############################ run_cycle_pkix ############################
# run test suites with PKIX enabled
########################################################################
run_cycle_pkix()
{
    TEST_MODE=PKIX

    TABLE_ARGS="bgcolor=cyan"
    html_head "Testing with PKIX"
    html "</TABLE><BR>"

    HOSTDIR="${HOSTDIR}/pkix"
    mkdir -p "${HOSTDIR}"
    init_directories

    NSS_ENABLE_PKIX_VERIFY="1"
    export NSS_ENABLE_PKIX_VERIFY

    TESTS="${ALL_TESTS}"
    TESTS_SKIP="cipher dbtests sdr crmf smime merge multinit"

    echo "${NSS_SSL_TESTS}" | grep "_" > /dev/null
    RET=$?
    NSS_SSL_TESTS=`echo "${NSS_SSL_TESTS}" | sed -e "s/normal//g" -e "s/bypass//g" -e "s/fips//g" -e "s/_//g"`
    [ ${RET} -eq 0 ] && NSS_SSL_TESTS="${NSS_SSL_TESTS} bypass_bypass"

    run_tests
}

######################### run_cycle_upgrade_db #########################
# upgrades certificate database to shareable format and run test suites
# with those databases
########################################################################
run_cycle_upgrade_db()
{
    TEST_MODE=UPGRADE_DB

    TABLE_ARGS="bgcolor=pink"
    html_head "Testing with upgraded library"
    html "</TABLE><BR>"

    OLDHOSTDIR="${HOSTDIR}"
    HOSTDIR="${HOSTDIR}/upgradedb"
    mkdir -p "${HOSTDIR}"
    init_directories

    if [ -r "${OLDHOSTDIR}/cert.log" ]; then
        DIRS="alicedir bobdir CA cert_extensions client clientCA dave eccurves eve ext_client ext_server fips SDR server serverCA stapling tools/copydir cert.log cert.done tests.*"
        for i in $DIRS
        do
            cp -r ${OLDHOSTDIR}/${i} ${HOSTDIR} #2> /dev/null
        done
    fi

    # upgrade certs dbs to shared db 
    TESTS="dbupgrade"
    TESTS_SKIP=

    run_tests

    NSS_DEFAULT_DB_TYPE="sql"
    export NSS_DEFAULT_DB_TYPE

    # run the subset of tests with the upgraded database
    TESTS="${ALL_TESTS}"
    TESTS_SKIP="cipher libpkix cert dbtests sdr ocsp pkits chains"

    echo "${NSS_SSL_TESTS}" | grep "_" > /dev/null
    RET=$?
    NSS_SSL_TESTS=`echo "${NSS_SSL_TESTS}" | sed -e "s/normal//g" -e "s/bypass//g" -e "s/fips//g" -e "s/_//g"`
    [ ${RET} -eq 0 ] && NSS_SSL_TESTS="${NSS_SSL_TESTS} bypass_bypass"
    NSS_SSL_RUN=`echo "${NSS_SSL_RUN}" | sed -e "s/cov//g" -e "s/auth//g"`

    run_tests
}

########################## run_cycle_shared_db #########################
# run test suites with certificate databases set to shareable format
########################################################################
run_cycle_shared_db()
{
    TEST_MODE=SHARED_DB

    TABLE_ARGS="bgcolor=yellow"
    html_head "Testing with shared library"
    html "</TABLE><BR>"

    HOSTDIR="${HOSTDIR}/sharedb"
    mkdir -p "${HOSTDIR}"
    init_directories

    NSS_DEFAULT_DB_TYPE="sql"
    export NSS_DEFAULT_DB_TYPE

    # run the tests for native sharedb support
    TESTS="${ALL_TESTS}"
    TESTS_SKIP="cipher libpkix dbupgrade sdr ocsp pkits"

    echo "${NSS_SSL_TESTS}" | grep "_" > /dev/null
    RET=$?
    NSS_SSL_TESTS=`echo "${NSS_SSL_TESTS}" | sed -e "s/normal//g" -e "s/bypass//g" -e "s/fips//g" -e "s/_//g"`
    [ ${RET} -eq 0 ] && NSS_SSL_TESTS="${NSS_SSL_TESTS} bypass_bypass"
    NSS_SSL_RUN=`echo "${NSS_SSL_RUN}" | sed -e "s/cov//g" -e "s/auth//g"`

    run_tests
}

############################# run_cycles ###############################
# run test cycles defined in CYCLES variable
########################################################################
run_cycles()
{
    for CYCLE in ${CYCLES}
    do
        case "${CYCLE}" in 
        "standard")
            run_cycle_standard
            ;;
        "pkix")
            run_cycle_pkix
            ;;
        "upgradedb")
            run_cycle_upgrade_db
            ;;
        "sharedb")
            run_cycle_shared_db
            ;;
        esac  
        . ${ENV_BACKUP}
    done
}

############################## main code ###############################

cycles="standard pkix upgradedb sharedb"
CYCLES=${NSS_CYCLES:-$cycles}

tests="cipher lowhash libpkix cert dbtests tools fips sdr crmf smime ssl ocsp merge pkits chains"
TESTS=${NSS_TESTS:-$tests}

ALL_TESTS=${TESTS}

nss_ssl_tests="crl bypass_normal normal_bypass fips_normal normal_fips iopr"
NSS_SSL_TESTS="${NSS_SSL_TESTS:-$nss_ssl_tests}"

nss_ssl_run="cov auth stapling stress"
NSS_SSL_RUN="${NSS_SSL_RUN:-$nss_ssl_run}"

SCRIPTNAME=all.sh
CLEANUP="${SCRIPTNAME}"
cd `dirname $0`

# all.sh should be the first one to try to source the init 
if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
    cd common
    . ./init.sh
fi

# NOTE:
# Since in make at the top level, modutil is the last file
# created, we check for modutil to know whether the build
# is complete. If a new file is created after that, the 
# following test for modutil should check for that instead.
# Exception: when building softoken only, shlibsign is the
# last file created.
if [ ${NSS_BUILD_SOFTOKEN_ONLY} -eq "1" ]; then
  LAST_FILE_BUILT=shlibsign
else
  LAST_FILE_BUILT=modutil
fi

if [ ! -f ${DIST}/${OBJDIR}/bin/${LAST_FILE_BUILT}${PROG_SUFFIX} ]; then
    echo "Build Incomplete. Aborting test." >> ${LOGFILE}
    html_head "Testing Initialization"
    Exit "Checking for build"
fi

# NOTE:
# Lists of enabled tests and other settings are stored to ${ENV_BACKUP}
# file and are are restored after every test cycle.

ENV_BACKUP=${HOSTDIR}/env.sh
env_backup > ${ENV_BACKUP}

if [ "${O_CRON}" = "ON" ]; then
    run_cycles >> ${LOGFILE}
else 
    run_cycles | tee -a ${LOGFILE}
fi

SCRIPTNAME=all.sh

. ${QADIR}/common/cleanup.sh

