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
#   ssl_gtests.sh- Gtest based unit tests for ssl
#   gtests.sh    - Gtest based unit tests for everything else
#   bogo.sh      - Bogo interop tests (disabled by default)
#                  https://boringssl.googlesource.com/boringssl/+/master/ssl/test/PORTING.md
#   interop.sh   - Interoperability tests (disabled by default)
#                  https://github.com/ekr/tls_interop
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

RUN_FIPS=""

############################## run_tests ###############################
# run test suites defined in TESTS variable, skip scripts defined in
# TESTS_SKIP variable
########################################################################
run_tests()
{
    echo "Running test cycle: ${TEST_MODE} ----------------------"
    echo "List of tests that will be executed: ${TESTS}"
    for TEST in ${TESTS}
    do
        # NOTE: the spaces are important. If you don't include
        # the spaces, then turning off ssl_gtests will also turn off ssl
        # tests.
        echo " ${TESTS_SKIP} " | grep " ${TEST} " > /dev/null
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
# run test suites with dbm database (no PKIX, no sharedb)
########################################################################
run_cycle_standard()
{
    TEST_MODE=STANDARD

    TESTS="${ALL_TESTS}"
    TESTS_SKIP="cipher libpkix sdr ocsp pkits"

    NSS_DEFAULT_DB_TYPE="dbm"
    export NSS_DEFAULT_DB_TYPE

    NSS_SSL_TESTS=`echo "${NSS_SSL_TESTS}" | sed -e "s/normal//g" -e "s/fips//g" -e "s/_//g"`
    NSS_SSL_RUN=`echo "${NSS_SSL_RUN}" | sed -e "s/cov//g" -e "s/auth//g"`

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

    NSS_SSL_TESTS=`echo "${NSS_SSL_TESTS}" | sed -e "s/normal//g" -e "s/fips//g" -e "s/_//g"`
    export -n NSS_SSL_RUN

    # use the default format. (unset for the shell, export -n for binaries)
    export -n NSS_DEFAULT_DB_TYPE
    unset NSS_DEFAULT_DB_TYPE

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
        DIRS="alicedir bobdir CA cert_extensions client clientCA dave eccurves eve ext_client ext_server $RUN_FIPS SDR server serverCA stapling tools/copydir cert.log cert.done tests.*"
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

    NSS_SSL_TESTS=`echo "${NSS_SSL_TESTS}" | sed -e "s/normal//g" -e "s/fips//g" -e "s/_//g"`
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
    TESTS_SKIP="dbupgrade"

    export -n NSS_SSL_TESTS
    export -n NSS_SSL_RUN

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
            if [ -z "$NSS_DISABLE_LIBPKIX" ]; then
                run_cycle_pkix
            fi
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

SCRIPTNAME=all.sh
CLEANUP="${SCRIPTNAME}"
cd `dirname $0`

# all.sh should be the first one to try to source the init
if [ -z "${INIT_SOURCED}" -o "${INIT_SOURCED}" != "TRUE" ]; then
    cd common
    . ./init.sh
fi

cycles="standard pkix upgradedb sharedb"
CYCLES=${NSS_CYCLES:-$cycles}

NO_INIT_SUPPORT=`certutil --build-flags |grep -cw NSS_NO_INIT_SUPPORT`
if [ $NO_INIT_SUPPORT -eq 0 ]; then
    RUN_FIPS="fips"
fi

tests="cipher lowhash libpkix cert dbtests tools $RUN_FIPS sdr crmf smime ssl ocsp merge pkits ec gtests ssl_gtests"
# Don't run chains tests when we have a gyp build.
if [ "$OBJDIR" != "Debug" -a "$OBJDIR" != "Release" ]; then
  tests="$tests chains"
fi
TESTS=${NSS_TESTS:-$tests}

ALL_TESTS=${TESTS}

nss_ssl_tests="crl iopr policy"
if [ $NO_INIT_SUPPORT -eq 0 ]; then
    nss_ssl_tests="$nss_ssl_tests fips_normal normal_fips"
fi
NSS_SSL_TESTS="${NSS_SSL_TESTS:-$nss_ssl_tests}"

nss_ssl_run="cov auth stapling stress"
NSS_SSL_RUN="${NSS_SSL_RUN:-$nss_ssl_run}"

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
