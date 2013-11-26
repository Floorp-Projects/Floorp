#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# PRIOR TO RUNNING THIS SCRIPT
# you should adjust MAIL_COMMAND and QA_LIST
#
# External dependencies:
# - install the NISCC test files, e.g. at /niscc (readonly OK)
# - libfaketimeMT because the test certificates have expired
# - build environment for building NSS
# - gdb to analyze core files
# - a command line mail tool (e.g. mailx)
# - openssl to combine input PEM files into pkcs#12
# - curl for obtaining version information from the web
#

################################################################################
# Print script usage
################################################################################
usage()
{
    cat << EOF
Usage: $0 [options]

Test NSS library against NISCC SMIME and TLS testcases.

Options:
 -h, --help           print this help message and exit
 -v, --verbose        enable extra verbose output
     --niscc-home DIR use NISCC testcases from directory DIR (default /niscc)
     --host HOST      use host HOST (default '127.0.0.1')
     --threads X      set thread number to X (max. 10, default 10)
     --out DIR        set DIR as output directory (default '/out')
     --mail ADDRESS   send mail with test result to ADDRESS
     --nss DIR        set NSS directory to DIR (default '~/niscc-hg/nss')
     --nss-hack DIR   set hacked NSS directory to DIR (default '~/niscc-hg/nss_hack')
     --log-store      store all the logs (only summary by default)
     --no-build-test  don't pull and build tested NSS
     --no-build-hack  don't pull and build hacked NSS
     --test-system    test system installed NSS
     --date DATE      use DATE in log archive name and outgoing email
     --libfaketime path.so  use faketime library with LD_PRELOAD=path.so
     --smallset       test only a very small subset

All options are optional.
All options (and possibly more) can be also set through environment variables.
Commandline options have higher priority than environment variables.
For more information please refer to the source code of this script.

For a successfull run the script NEEDS the core file pattern to be 'core.*',
e.g. 'core.%t'. You can check the current pattern in
'/proc/sys/kernel/core_pattern'. Otherwise the test will be unable to detect
any failures and will pass every time.

It is recommended to use hacked and tested binaries in a location, where their
absolute path is max. 80 characters. If their path is longer and a core file is
generated, its properties may be incomplete.

Return value of the script indicates how many failures it experienced.

EOF
    exit $1
}

################################################################################
# Process command-line arguments
################################################################################
process_args()
{
    HELP="false"
    args=`getopt -u -l "niscc-home:,host:,threads:,out:,verbose,mail:,nss:,nss-hack:,log-store,no-build-test,no-build-hack,help,test-system,date:,libfaketime:,smallset" -- "hv" $*`
    [ "$?" != "0" ] && usage 1
    set -- $args
    for i; do
        case "$i" in
            -v|--verbose)
                shift
                VERBOSE="-v"
                ;;
            --niscc-home)
                shift
                NISCC_HOME="$1"
                shift
                ;;
            --host)
                shift
                HOST="$1"
                shift
                ;;
            --threads)
                shift
                THREADS="$1"
                shift
                ;;
            --out)
                shift
                TEST_OUTPUT="$1"
                shift
                ;;
            --mail)
                shift
                USE_MAIL="true"
                QA_LIST="$1"
                shift
                ;;
            --nss)
                shift
                LOCALDIST="$1"
                shift
                ;;
            --nss-hack)
                shift
                NSS_HACK="$1"
                shift
                ;;
            --log-store)
                shift
                LOG_STORE="true"
                ;;
            --no-build-test)
                shift
                NO_BUILD_TEST="true"
                ;;
            --no-build-hack)
                shift
                NO_BUILD_HACK="true"
                ;;
            -h|--help)
                shift
                HELP="true"
                ;;
            --test-system)
                shift
                TEST_SYSTEM="true"
                ;;
            --date)
                shift
                DATE="$1"
                shift
                ;;
            --libfaketime)
                shift
                FAKETIMELIB="$1"
                shift
                ;;
            --smallset)
                shift
                SMALLSET="true"
                ;;
            --)
                ;;
            *)
                ;;
        esac
    done
    [ $HELP = "true" ] && usage 0
}

################################################################################
# Create and set needed and useful environment variables
################################################################################
create_environment()
{
    # Base location of NISCC testcases
    export NISCC_HOME=${NISCC_HOME:-/niscc}

    # Base location of NSS
    export HG=${HG:-"$HOME/niscc-hg"}

    # NSS being tested
    export LOCALDIST=${LOCALDIST:-"${HG}/nss"}

    # Hacked NSS - built with "NISCC_TEST=1"
    export NSS_HACK=${NSS_HACK:-"${HG}/nss_hack"}

    # Hostname of the testmachine
    export HOST=${HOST:-127.0.0.1}

    # Whether to store logfiles
    export LOG_STORE=${LOG_STORE:-"false"}

    # Whether to mail the summary
    export USE_MAIL=${USE_MAIL:-"false"}

    # How to mail summary
    export MAIL_COMMAND=${MAIL_COMMAND:-"mailx -S smtp=smtp://your.smtp.server:25 -r your+niscc@email.address"}

    # List of mail addresses where to send summary
    export QA_LIST=${QA_LIST:-"result@recipient.address"}

    # Whether to use 64b build
    export USE_64=${USE_64:-1}

    # Directory where to write all the output data (around 650MiB for each run)
    export TEST_OUTPUT=${TEST_OUTPUT:-"$HOME/out"}

    # How many threads to use in selfserv and strsclnt (max. 10)
    export THREADS=${THREADS:-10}

    # If true, do not build tthe tested version of NSS
    export NO_BUILD_TEST=${NO_BUILD_TEST:-"false"}

    # If true, do not build the special NSS version for NISCC
    export NO_BUILD_HACK=${NO_BUILD_HACK:-"false"}

    # If true, do not rebuild client and server directories
    export NO_SETUP=${NO_SETUP:-"false"}

    # Location of NISCC SSL/TLS testcases
    export TEST=${TEST:-"${NISCC_HOME}/NISCC_SSL_testcases"}

    # If true, then be extra verbose
    export VERBOSE=${VERBOSE:-""}

    # If true, test the system installed NSS
    export TEST_SYSTEM=${TEST_SYSTEM:-"false"}
    [ "$TEST_SYSTEM" = "true" ] && export NO_BUILD_TEST="true"

    [ ! -z "$VERBOSE" ] && set -xv

    # Real date for naming of archives (system date must be 2002-11-18 .. 2007-11-18 due to certificate validity
    DATE=${DATE:-`date`}
    export DATE=`date -d "$DATE" +%Y%m%d`

    FAKETIMELIB=${FAKETIMELIB:-""}
    export DATE=`date -d "$DATE" +%Y%m%d`

    # Whether to test only a very small subset
    export SMALLSET=${SMALLSET:-"false"}

    # Create output dir if it doesn't exist
    mkdir -p ${TEST_OUTPUT}
}

################################################################################
# Do a HG pull of NSS
################################################################################
hg_pull()
{
    # Tested NSS - by default using HG default tip
    if [ "$NO_BUILD_TEST" = "false" ]; then
        echo "cloning NSS sources to be tested from HG"
        [ ! -d "$LOCALDIST" ] && mkdir -p "$LOCALDIST"
        cd "$LOCALDIST"
        [ ! -d "$LOCALDIST/nspr" ] && hg clone --noupdate https://hg.mozilla.org/projects/nspr
        cd nspr; hg pull; hg update -C -r default; cd ..
        [ ! -d "$LOCALDIST/nss" ] && hg clone --noupdate https://hg.mozilla.org/projects/nss
        cd nss; hg pull; hg update -C -r default; cd ..
        #find . -exec touch {} \;
    fi

    # Hacked NSS - by default using some RTM version.
    # Do not use HEAD for hacked NSS - it needs to be stable and bug-free
    if [ "$NO_BUILD_HACK" = "false" ]; then
        echo "cloning NSS sources for a hacked build from HG"
        [ ! -d "$NSS_HACK" ] && mkdir -p "$NSS_HACK"
        cd "$NSS_HACK"
        NSPR_TAG=`curl --silent http://hg.mozilla.org/releases/mozilla-aurora/raw-file/default/nsprpub/TAG-INFO | head -1 | sed --regexp-extended 's/[[:space:]]//g' | awk '{print $1}'`
        NSS_TAG=`curl --silent http://hg.mozilla.org/releases/mozilla-aurora/raw-file/default/security/nss/TAG-INFO | head -1 | sed --regexp-extended 's/[[:space:]]//g' | awk '{print $1}'`
        [ ! -d "$NSS_HACK/nspr" ] && hg clone --noupdate https://hg.mozilla.org/projects/nspr
        cd nspr; hg pull; hg update -C -r "$NSPR_TAG"; cd ..
        [ ! -d "$NSS_HACK/nss" ] && hg clone --noupdate https://hg.mozilla.org/projects/nss
        cd nss; hg pull; hg update -C -r "$NSS_TAG"; cd ..
        #find . -exec touch {} \;
    fi
}

################################################################################
# Build NSS after setting make variable NISCC_TEST
################################################################################
build_NSS()
{
    # Tested NSS
    if [ "$NO_BUILD_TEST" = "false" ]; then
        echo "building NSS to be tested"
        cd "$LOCALDIST"
        unset NISCC_TEST
        cd nss
        gmake nss_clean_all &>> $TEST_OUTPUT/nisccBuildLog
        gmake nss_build_all &>> $TEST_OUTPUT/nisccBuildLog
    fi

    # Hacked NSS
    if [ "$NO_BUILD_HACK" = "false" ]; then
        echo "building hacked NSS"
        cd "$NSS_HACK"
        export NISCC_TEST=1
        cd nss
        gmake nss_clean_all &>> $TEST_OUTPUT/nisccBuildLogHack
        gmake nss_build_all &>> $TEST_OUTPUT/nisccBuildLogHack
    fi

    unset NISCC_TEST
}

################################################################################
# Set build dir, bin and lib directories
################################################################################
init()
{
    # Enable useful core files to be generated in case of crash
    ulimit -c unlimited

    # Pattern of core files, they should be created in current directory
    echo "core_pattern $(cat /proc/sys/kernel/core_pattern)" > "$TEST_OUTPUT/nisccLog00"

    # gmake is needed in the path for this suite to run
    echo "PATH $PATH" >> "$TEST_OUTPUT/nisccLog00"

    # Find out hacked NSS version
    DISTTYPE=`cd "$NSS_HACK/nss/tests/common"; gmake objdir_name`
    echo "NSS_HACK DISTTYPE $DISTTYPE" >> "$TEST_OUTPUT/nisccLog00"
    export HACKBIN="$NSS_HACK/dist/$DISTTYPE/bin"
    export HACKLIB="$NSS_HACK/dist/$DISTTYPE/lib"

    if [ "$TEST_SYSTEM" = "false" ]; then
        # Find out nss version
        DISTTYPE=`cd "$LOCALDIST/nss/tests/common"; gmake objdir_name`
        echo "NSS DISTTYPE $DISTTYPE" >> "$TEST_OUTPUT/nisccLog00"
        export TESTBIN="$LOCALDIST/dist/$DISTTYPE/bin"
        export TESTLIB="$LOCALDIST/dist/$DISTTYPE/lib"
        export TESTTOOLS="$TESTBIN"
    else
        # Using system installed NSS
        echo "USING SYSTEM NSS" >> "$TEST_OUTPUT/nisccLog00"
        export TESTBIN="/usr/bin"
        if [ `uname -m` = "x86_64" ]; then
            export TESTLIB="/usr/lib64"
            export TESTTOOLS="/usr/lib64/nss/unsupported-tools"
        else
            export TESTLIB="/usr/lib"
            export TESTTOOLS="/usr/lib/nss/unsupported-tools"
        fi
    fi

    # Verify NISCC_TEST was set in the proper library
    if strings "$HACKLIB/libssl3.so" | grep NISCC_TEST > /dev/null 2>&1; then
        echo "$HACKLIB/libssl3.so contains NISCC_TEST" >> "$TEST_OUTPUT/nisccLog00"
    else
        echo "$HACKLIB/libssl3.so does NOT contain NISCC_TEST" >> "$TEST_OUTPUT/nisccLog00"
    fi

    if strings "$TESTLIB/libssl3.so" | grep NISCC_TEST > /dev/null 2>&1; then
        echo "$TESTLIB/libssl3.so contains NISCC_TEST" >> "$TEST_OUTPUT/nisccLog00"
    else
        echo "$TESTLIB/libssl3.so does NOT contain NISCC_TEST" >> "$TEST_OUTPUT/nisccLog00"
    fi
}

################################################################################
# Setup simple client and server directory
################################################################################
ssl_setup_dirs_simple()
{
    [ "$NO_SETUP" = "true" ] && return

    echo "Setting up working directories for SSL simple tests"

    CLIENT="$TEST_OUTPUT/niscc_ssl/simple_client"
    SERVER="$TEST_OUTPUT/niscc_ssl/simple_server"

    # Generate .p12 files
    openssl pkcs12 -export -inkey "$TEST/client_key.pem" -in "$TEST/client_crt.pem" -out "$TEST_OUTPUT/client_crt.p12" -passout pass:testtest1 -name "client_crt"
    openssl pkcs12 -export -inkey "$TEST/server_key.pem" -in "$TEST/server_crt.pem" -out "$TEST_OUTPUT/server_crt.p12" -passout pass:testtest1 -name "server_crt"

    # Setup simple client directory
    rm -rf "$CLIENT"
    mkdir -p "$CLIENT"
    echo test > "$CLIENT/password-is-test.txt"
    export LD_LIBRARY_PATH="$TESTLIB"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -N -d "$CLIENT" -f "$CLIENT/password-is-test.txt" >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -A -d "$CLIENT" -n rootca -i "$TEST/rootca.crt" -t "C,C," >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/pk12util" -i "$TEST_OUTPUT/client_crt.p12" -d "$CLIENT" -k "$CLIENT/password-is-test.txt" -W testtest1 >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -L -d "$CLIENT" >> "$TEST_OUTPUT/nisccLog00" 2>&1

    # File containg message used for terminating the server
    echo "GET /stop HTTP/1.0" > "$CLIENT/stop.txt"
    echo ""                  >> "$CLIENT/stop.txt"

    # Setup simple server directory
    rm -rf "$SERVER"
    mkdir -p "$SERVER"
    echo test > "$SERVER/password-is-test.txt"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -N -d "$SERVER" -f "$SERVER/password-is-test.txt" >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -A -d "$SERVER" -n rootca -i "$TEST/rootca.crt" -t "TC,C," >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/pk12util" -i "$TEST_OUTPUT/server_crt.p12" -d "$SERVER" -k "$SERVER/password-is-test.txt" -W testtest1 >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -L -d "$SERVER" >> "$TEST_OUTPUT/nisccLog00" 2>&1

    unset LD_LIBRARY_PATH
}

################################################################################
# Setup resigned client and server directory
################################################################################
ssl_setup_dirs_resigned()
{
    [ "$NO_SETUP" = "true" ] && return

    echo "Setting up working directories for SSL resigned tests"

    CLIENT="$TEST_OUTPUT/niscc_ssl/resigned_client"
    SERVER="$TEST_OUTPUT/niscc_ssl/resigned_server"

    # Setup resigned client directory
    rm -rf "$CLIENT"
    mkdir -p "$CLIENT"
    echo test > "$CLIENT/password-is-test.txt"
    export LD_LIBRARY_PATH="$TESTLIB"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -N -d "$CLIENT" -f "$CLIENT/password-is-test.txt" >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -A -d "$CLIENT" -n rootca -i "$TEST/rootca.crt" -t "C,C," >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/pk12util" -i "$TEST_OUTPUT/client_crt.p12" -d "$CLIENT" -k "$CLIENT/password-is-test.txt" -W testtest1 >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -L -d "$CLIENT" >> "$TEST_OUTPUT/nisccLog00" 2>&1

    echo "GET /stop HTTP/1.0" > "$CLIENT/stop.txt"
    echo ""                  >> "$CLIENT/stop.txt"

    # Setup resigned server directory
    rm -rf "$SERVER"
    mkdir -p "$SERVER"
    echo test > "$SERVER/password-is-test.txt"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -N -d "$SERVER" -f "$SERVER/password-is-test.txt" >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -A -d "$SERVER" -n rootca -i "$TEST/rootca.crt" -t "TC,C," >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/pk12util" -i "$TEST_OUTPUT/server_crt.p12" -d "$SERVER" -k "$SERVER/password-is-test.txt" -W testtest1 >> "$TEST_OUTPUT/nisccLog00" 2>&1
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/certutil" -L -d "$SERVER" >> "$TEST_OUTPUT/nisccLog00" 2>&1

    unset LD_LIBRARY_PATH
}

################################################################################
# NISCC SMIME tests
################################################################################
niscc_smime()
{
    cd "$TEST_OUTPUT"
    DATA="$NISCC_HOME/NISCC_SMIME_testcases"

    [ ! -d niscc_smime ] && mkdir -p niscc_smime

    export SMIME_CERT_DB_DIR=envDB
    export NSS_STRICT_SHUTDOWN=1
    export NSS_DISABLE_ARENA_FREE_LIST=1
    export LD_LIBRARY_PATH="$TESTLIB"

    # Generate .p12 files
    openssl pkcs12 -export -inkey "$DATA/Client.key" -in "$DATA/Client.crt" -out Client.p12 -passout pass:testtest1 &>/dev/null
    openssl pkcs12 -export -inkey "$DATA/CA.key" -in "$DATA/CA.crt" -out CA.p12 -passout pass:testtest1 &>/dev/null

    # Generate envDB if needed
    if [ ! -d "$SMIME_CERT_DB_DIR" ]; then
        mkdir -p "$SMIME_CERT_DB_DIR"
        echo testtest1 > password-is-testtest1.txt
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTBIN}/certutil" -N -d "./$SMIME_CERT_DB_DIR" -f password-is-testtest1.txt > /dev/null 2>&1
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTBIN}/certutil" -A -d "$SMIME_CERT_DB_DIR" -f password-is-testtest1.txt -i "$DATA/CA.crt" -n CA -t "TC,C,"
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTBIN}/certutil" -A -d "$SMIME_CERT_DB_DIR" -f password-is-testtest1.txt -i "$DATA/Client.crt" -n Client -t "TC,C,"
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTBIN}/pk12util" -i ./CA.p12 -d "$SMIME_CERT_DB_DIR" -k password-is-testtest1.txt -W testtest1
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTBIN}/pk12util" -i ./Client.p12 -d "$SMIME_CERT_DB_DIR" -k password-is-testtest1.txt -W testtest1
    fi

    # if p7m-ed-m-files.txt does not exist, then generate it.
    [ -f "$DATA/p7m-ed-m-files.txt" ] && sed "s|^|$DATA/|" "$DATA/p7m-ed-m-files.txt" > p7m-ed-m-files.txt
    export P7M_ED_M_FILES=p7m-ed-m-files.txt
    if [ "$SMALLSET" = "true" ]; then
        [ ! -f "$P7M_ED_M_FILES" ] && find "$DATA"/p7m-ed-m-0* -type f -print | head -10 >> "$P7M_ED_M_FILES"
    else
        [ ! -f "$P7M_ED_M_FILES" ] && find "$DATA"/p7m-ed-m-0* -type f -print >> "$P7M_ED_M_FILES"
    fi

    # Test "p7m-ed-m*" testcases
    echo "Testing SMIME enveloped data testcases"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/cmsutil" $VERBOSE -D -d "$SMIME_CERT_DB_DIR" -p testtest1 -b -i "$P7M_ED_M_FILES" > niscc_smime/p7m-ed-m-results.txt 2>&1

    export SMIME_CERT_DB_DIR=sigDB
    # Generate sigDB if needed
    if [ ! -d "$SMIME_CERT_DB_DIR" ]; then
        mkdir -p "$SMIME_CERT_DB_DIR"
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTBIN}/certutil" -N -d "$SMIME_CERT_DB_DIR" -f password-is-testtest1.txt
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTBIN}/certutil" -A -d "$SMIME_CERT_DB_DIR" -i "$DATA/CA.crt" -n CA -t "TC,C,"
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTBIN}/certutil" -A -d "$SMIME_CERT_DB_DIR" -i "$DATA/Client.crt" -n Client -t "TC,C,"
    fi

    # if p7m-sd-dt-files.txt does not exist, then generate it.
    [ -f "$DATA/p7m-sd-dt-files.txt" ] && sed "s|^|$DATA/|" "$DATA/p7m-sd-dt-files.txt" > p7m-sd-dt-files.txt
    export P7M_SD_DT_FILES=p7m-sd-dt-files.txt
    if [ "$SMALLSET" = "true" ]; then
        [ ! -f "$P7M_SD_DT_FILES" ] && find "$DATA"/p7m-sd-dt-[cm]-* -type f -print | head -10 >> "$P7M_SD_DT_FILES"
    else
        [ ! -f "$P7M_SD_DT_FILES" ] && find "$DATA"/p7m-sd-dt-[cm]-* -type f -print >> "$P7M_SD_DT_FILES"
    fi

    [ ! -f detached.txt ] && touch detached.txt

    # Test "p7m-sd-dt*" testcases
    echo "Testing SMIME detached signed data testcases"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/cmsutil" $VERBOSE -D -d "$SMIME_CERT_DB_DIR" -c detached.txt -b -i "$P7M_SD_DT_FILES" > niscc_smime/p7m-sd-dt-results.txt 2>&1

    # if p7m-sd-op-files.txt does not exist, then generate it.
    [ -f "$DATA/p7m-sd-op-files.txt" ] && sed "s|^|$DATA/|" "$DATA/p7m-sd-op-files.txt" > p7m-sd-op-files.txt
    export P7M_SD_OP_FILES=p7m-sd-op-files.txt
    if [ "$SMALLSET" = "true" ]; then
        [ ! -f "$P7M_SD_OP_FILES" ] && find "$DATA"/p7m-sd-op-[cm]-* -type f -print | head -10 >> "$P7M_SD_OP_FILES"
    else
        [ ! -f "$P7M_SD_OP_FILES" ] && find "$DATA"/p7m-sd-op-[cm]-* -type f -print >> "$P7M_SD_OP_FILES"
    fi

    # Test "p7m-sd-op*" testcases
    echo "Testing SMIME opaque signed data testcases"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTBIN}/cmsutil" $VERBOSE -D -d "$SMIME_CERT_DB_DIR" -b -i "$P7M_SD_OP_FILES" > niscc_smime/p7m-sd-op-results.txt 2>&1

    unset LD_LIBRARY_PATH
}

################################################################################
# Set env variables for NISCC SSL tests
################################################################################
niscc_ssl_init()
{
    export NSS_STRICT_SHUTDOWN=1
    export NSS_DISABLE_ARENA_FREE_LIST=1
    cd "$TEST_OUTPUT"
}

force_crash()
{
    echo "int main(int argc, char *argv[]) { int *i; i = (int*)(void*)1; *i = 1; }" > "$TEST_OUTPUT/crashme.c"
    gcc -g -o "$TEST_OUTPUT/crashme" "$TEST_OUTPUT/crashme.c"
    "$TEST_OUTPUT/crashme"
}

################################################################################
# Do simple client auth tests
# Use an altered client against the server
################################################################################
ssl_simple_client_auth()
{
    echo "Testing SSL simple client auth testcases"
    export CLIENT="$TEST_OUTPUT/niscc_ssl/simple_client"
    export SERVER="$TEST_OUTPUT/niscc_ssl/simple_server"
    export PORT=8443
    export START_AT=1
    if [ "$SMALLSET" = "true" ]; then
        export STOP_AT=10
    else
        export STOP_AT=106160
    fi
    unset NISCC_TEST
    export LD_LIBRARY_PATH="$TESTLIB"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTTOOLS}/selfserv" $VERBOSE -p $PORT -d "$SERVER" -n server_crt -rr -t $THREADS -w test > "$TEST_OUTPUT/nisccLog01" 2>&1 &

    export NISCC_TEST="$TEST/simple_client"
    export LD_LIBRARY_PATH="$HACKLIB"

    for START in `seq $START_AT $THREADS $STOP_AT`; do
        START_AT=$START \
        STOP_AT=$(($START+$THREADS)) \
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${HACKBIN}/strsclnt" $VERBOSE -d "$CLIENT" -n client_crt -p $PORT -t $THREADS -c $THREADS -o -N -w test $HOST >> "$TEST_OUTPUT/nisccLog02" 2>&1
    done

    unset NISCC_TEST
    echo "starting tstclnt to shutdown simple client selfserv process"
    for i in `seq 5`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${HACKBIN}/tstclnt" -h $HOST -p $PORT -d "$CLIENT" -n client_crt -o -f -w test < "$CLIENT/stop.txt" >> "$TEST_OUTPUT/nisccLog02" 2>&1
    done

    unset LD_LIBRARY_PATH

    sleep 1
}

################################################################################
# Do simple server auth tests
# Use an altered server against the client
################################################################################
ssl_simple_server_auth()
{
    echo "Testing SSL simple server auth testcases"
    export CLIENT="$TEST_OUTPUT/niscc_ssl/simple_client"
    export SERVER="$TEST_OUTPUT/niscc_ssl/simple_server"
    export PORT=8444
    export START_AT=00000001
    if [ "$SMALLSET" = "true" ]; then
        export STOP_AT=00000010
    else
        export STOP_AT=00106167
    fi
    export LD_LIBRARY_PATH="$HACKLIB"
    export NISCC_TEST="$TEST/simple_server"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${HACKBIN}/selfserv" $VERBOSE -p $PORT -d "$SERVER" -n server_crt -t $THREADS -w test > "$TEST_OUTPUT/nisccLog03" 2>&1 &

    unset NISCC_TEST
    export LD_LIBRARY_PATH="$TESTLIB"
    for START in `seq $START_AT $THREADS $STOP_AT`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTTOOLS}/strsclnt" $VERBOSE -d "$CLIENT" -p $PORT -t $THREADS -c $THREADS -o -N $HOST >> "$TEST_OUTPUT/nisccLog04" 2>&1
    done

    echo "starting tstclnt to shutdown simple server selfserv process"
    for i in `seq 5`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTTOOLS}/tstclnt" -h $HOST -p $PORT -d "$CLIENT" -n client_crt -o -f -w test < "$CLIENT/stop.txt" >> "$TEST_OUTPUT/nisccLog04" 2>&1
    done

    unset LD_LIBRARY_PATH

    sleep 1
}

################################################################################
# Do simple rootCA tests
# Use an altered server against the client
################################################################################
ssl_simple_rootca()
{
    echo "Testing SSL simple rootCA testcases"
    export CLIENT="$TEST_OUTPUT/niscc_ssl/simple_client"
    export SERVER="$TEST_OUTPUT/niscc_ssl/simple_server"
    export PORT=8445
    export START_AT=1
    if [ "$SMALLSET" = "true" ]; then
        export STOP_AT=10
    else
        export STOP_AT=106190
    fi
    export LD_LIBRARY_PATH="$HACKLIB"
    export NISCC_TEST="$TEST/simple_rootca"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${HACKBIN}/selfserv" $VERBOSE -p $PORT -d "$SERVER" -n server_crt -t $THREADS -w test > "$TEST_OUTPUT/nisccLog05" 2>&1 &

    unset NISCC_TEST
    export LD_LIBRARY_PATH="$TESTLIB"
    for START in `seq $START_AT $THREADS $STOP_AT`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTTOOLS}/strsclnt" $VERBOSE -d "$CLIENT" -p $PORT -t $THREADS -c $THREADS -o -N $HOST >> "$TEST_OUTPUT/nisccLog06" 2>&1
    done

    echo "starting tstclnt to shutdown simple rootca selfserv process"
    for i in `seq 5`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTTOOLS}/tstclnt" -h $HOST -p $PORT -d "$CLIENT" -n client_crt -o -f -w test < "$CLIENT/stop.txt" >> "$TEST_OUTPUT/nisccLog06" 2>&1
    done

    unset LD_LIBRARY_PATH

    sleep 1
}

################################################################################
# Do resigned client auth tests
# Use an altered client against the server
################################################################################
ssl_resigned_client_auth()
{
    echo "Testing SSL resigned client auth testcases"
    export CLIENT="$TEST_OUTPUT/niscc_ssl/resigned_client"
    export SERVER="$TEST_OUTPUT/niscc_ssl/resigned_server"
    export PORT=8446
    export START_AT=0
    if [ "$SMALLSET" = "true" ]; then
        export STOP_AT=9
    else
        export STOP_AT=99981
    fi
    unset NISCC_TEST
    export LD_LIBRARY_PATH="$TESTLIB"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${TESTTOOLS}/selfserv" $VERBOSE -p $PORT -d "$SERVER" -n server_crt -rr -t $THREADS -w test > "$TEST_OUTPUT/nisccLog07" 2>&1 &

    export NISCC_TEST="$TEST/resigned_client"
    export LD_LIBRARY_PATH="$HACKLIB"

    for START in `seq $START_AT $THREADS $STOP_AT`; do
        START_AT=$START \
        STOP_AT=$(($START+$THREADS)) \
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${HACKBIN}/strsclnt" $VERBOSE -d "$CLIENT" -n client_crt -p $PORT -t $THREADS -c $THREADS -o -N -w test $HOST >> "$TEST_OUTPUT/nisccLog08" 2>&1
    done

    unset NISCC_TEST
    echo "starting tstclnt to shutdown resigned client selfserv process"
    for i in `seq 5`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${HACKBIN}/tstclnt" -h $HOST -p $PORT -d "$CLIENT" -n client_crt -o -f -w test < "$CLIENT/stop.txt" >> "$TEST_OUTPUT/nisccLog08" 2>&1
    done

    unset LD_LIBRARY_PATH

    sleep 1
}

################################################################################
# Do resigned server auth tests
# Use an altered server against the client
################################################################################
ssl_resigned_server_auth()
{
    echo "Testing SSL resigned server auth testcases"
    export CLIENT="$TEST_OUTPUT/niscc_ssl/resigned_client"
    export SERVER="$TEST_OUTPUT/niscc_ssl/resigned_server"
    export PORT=8447
    export START_AT=0
    if [ "$SMALLSET" = "true" ]; then
        export STOP_AT=9
    else
        export STOP_AT=100068
    fi
    export LD_LIBRARY_PATH="$HACKLIB"
    export NISCC_TEST="$TEST/resigned_server"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${HACKBIN}/selfserv" $VERBOSE -p $PORT -d "$SERVER" -n server_crt -t $THREADS -w test > "$TEST_OUTPUT/nisccLog09" 2>&1 &

    unset NISCC_TEST
    export LD_LIBRARY_PATH="$TESTLIB"
    for START in `seq $START_AT $THREADS $STOP_AT`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTTOOLS}/strsclnt" $VERBOSE -d "$CLIENT" -p $PORT -t $THREADS -c $THREADS -o -N $HOST >> "$TEST_OUTPUT/nisccLog10" 2>&1
    done

    echo "starting tstclnt to shutdown resigned server selfserv process"
    for i in `seq 5`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTTOOLS}/tstclnt" -h $HOST -p $PORT -d "$CLIENT" -n client_crt -o -f -w test < "$CLIENT/stop.txt" >> "$TEST_OUTPUT/nisccLog10" 2>&1
    done

    unset LD_LIBRARY_PATH

    sleep 1
}

################################################################################
# Do resigned rootCA tests
# Use an altered server against the client
################################################################################
ssl_resigned_rootca()
{
    echo "Testing SSL resigned rootCA testcases"
    export CLIENT="$TEST_OUTPUT/niscc_ssl/resigned_client"
    export SERVER="$TEST_OUTPUT/niscc_ssl/resigned_server"
    export PORT=8448
    export START_AT=0
    if [ "$SMALLSET" = "true" ]; then
        export STOP_AT=9
    else
        export STOP_AT=99959
    fi
    export LD_LIBRARY_PATH="$HACKLIB"
    export NISCC_TEST="$TEST/resigned_rootca"
    LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
    "${HACKBIN}/selfserv" $VERBOSE -p $PORT -d "$SERVER" -n server_crt -t $THREADS -w test > "$TEST_OUTPUT/nisccLog11" 2>&1 &

    unset NISCC_TEST
    export LD_LIBRARY_PATH="$TESTLIB"
    for START in `seq $START_AT $THREADS $STOP_AT`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTTOOLS}/strsclnt" $VERBOSE -d "$CLIENT" -p $PORT -t $THREADS -c $THREADS -o -N $HOST >> "$TEST_OUTPUT/nisccLog12" 2>&1
    done

    echo "starting tstclnt to shutdown resigned rootca selfserv process"
    for i in `seq 5`; do
        LD_PRELOAD=${FAKETIMELIB} NO_FAKE_STAT=1 FAKETIME="@2004-03-29 14:14:14" \
        "${TESTTOOLS}/tstclnt" -h $HOST -p $PORT -d "$CLIENT" -n client_crt -o -f -w test < "$CLIENT/stop.txt" >> "$TEST_OUTPUT/nisccLog12" 2>&1
    done

    unset LD_LIBRARY_PATH

    sleep 1
}

################################################################################
# Email the test logfile, and if core found, notify of failure
################################################################################
mail_testLog()
{
    pushd "$TEST_OUTPUT"

    # remove mozilla nss build false positives and core stored in previous runs
    find . -name "core*" -print | grep -v coreconf | grep -v core_watch | grep -v archive >> crashLog
    export SIZE=`cat crashLog | wc -l`

    [ "$USE_MAIL" = "false" ] && return

    # mail text
    MT=mailText
    rm -f $MT

    if [ "$SIZE" -ne 1 ]; then
        echo "### FAILED ###" >> $MT
        echo "### Exactly one crash is expected." >> $MT
        echo "### Zero means: crash detection is broken, fix the script!" >> $MT
        echo "### > 1 means: robustness test failure, fix the bug! (check the logs)" >> $MT
        cat crashLog >> nisccLogSummary
        SUBJ="FAILED: NISCC TESTS (check file: crashLog)"
    else
        echo ":) PASSED :)" >> $MT
        SUBJ="PASSED: NISCC tests"
    fi

    echo "Date used during test run: $DATE" >> $MT

    echo "Count of lines in files:" >> $MT
    wc -l crashLog nisccBuildLog nisccBuildLogHack nisccLog[0-9]* p7m-* |grep -vw total >> $MT
    NUM=`cat nisccLog0[123456789] nisccLog1[12] | egrep -ic "success/passed"`
    echo "Number of times the SSL tests reported success/passed (low expected): $NUM" >> $MT
    NUM=`cat nisccLog0[123456789] nisccLog1[12] | egrep -ic "problem|failed|error"`
    echo "Number of times the SSL tests reported problem/failed/error (high expected): $NUM" >> $MT
    NUM=`cat niscc_smime/p7m*results.txt | egrep -ic "success/passed"`
    echo "Number of times the S/MIME tests reported success/passed (low expected): $NUM" >> $MT
    NUM=`cat niscc_smime/p7m*results.txt | egrep -ic "problem|failed|error"`
    echo "Number of times the S/MIME tests reported problem/failed/error (high expected): $NUM" >> $MT
    echo "==== tail of nisccBuildLog ====" >> $MT
    tail -20 nisccBuildLog >> $MT
    echo "===============================" >> $MT
    echo "==== tail of nisccBuildLogHack ====" >> $MT
    tail -20 nisccBuildLogHack >> $MT
    echo "===================================" >> $MT

    #NUM=``
    #echo "Number of : $NUM" >> $MT

    cat $MT | $MAIL_COMMAND -s "$SUBJ" $QA_LIST

    popd
}

################################################################################
# Summarize all logs
################################################################################
log_summary()
{
    echo "Summarizing all logs"
    # Move old logs
    [ -f "$TEST_OUTPUT/nisccLogSummary" ] && mv nisccLogSummary nisccLogSummary.old
    [ -f "$TEST_OUTPUT/crashLog" ] && mv crashLog crashLog.old

    for a in $TEST_OUTPUT/nisccLog[0-9]*; do
        echo ================================== "$a"
        grep -v using "$a" | sort | uniq -c | sort -b -n +0 -1
    done > $TEST_OUTPUT/nisccLogSummary

    for a in $TEST_OUTPUT/niscc_smime/p7m-*-results.txt; do
        echo ================================== "$a"
        grep -v using "$a" | sort | uniq -c | sort -b -n +0 -1
    done >> $TEST_OUTPUT/nisccLogSummary
}

################################################################################
# Process core files
################################################################################
core_process()
{
    echo "Processing core files"
    cd "$TEST_OUTPUT"

    for CORE in `cat crashLog`; do
        FILE=`file "$CORE" | sed "s/.* from '//" | sed "s/'.*//"`
        BINARY=`strings "$CORE" | grep "^${FILE}" | tail -1`
        gdb "$BINARY" "$CORE" << EOF_GDB > "$CORE.details"
where
quit
EOF_GDB
    done
}

################################################################################
# Move the old log files to save them, delete extra log files
################################################################################
move_files()
{
    echo "Moving and deleting log files"
    cd "$TEST_OUTPUT"

    rm -rf TRASH
    mkdir TRASH

    if [ "$LOG_STORE" = "true" ]; then
        BRANCH=`echo $LOCALDIST | sed "s:.*/\(security.*\)/builds/.*:\1:"`
        if [ "$BRANCH" = "$LOCALDIST" ]; then
            ARCHIVE="$TEST_OUTPUT/archive"
        else
            ARCHIVE="$TEST_OUTPUT/archive/$BRANCH"
        fi

        # Check for archive directory
        if [ ! -d "$ARCHIVE" ]; then
            mkdir -p "$ARCHIVE"
        fi

        # Determine next log storage point
        slot=`ls -1 "$ARCHIVE" | grep $DATE | wc -l`
        slot=`expr $slot + 1`
        location="$ARCHIVE/$DATE.$slot"
        mkdir -p "$location"

        # Archive the logs
        mv nisccBuildLog "$location" 2> /dev/null
        mv nisccBuildLogHack "$location" 2> /dev/null
        mv nisccLogSummary "$location"
        mv nisccLog* "$location"
        mv niscc_smime/p7m-ed-m-results.txt "$location"
        mv niscc_smime/p7m-sd-dt-results.txt "$location"
        mv niscc_smime/p7m-sd-op-results.txt "$location"

        # Archive any core files produced
        for core in `cat "$TEST_OUTPUT/crashLog"`; do
            mv "$core" "$location"
            mv "$core.details" "$location"
        done
        mv crashLog "$location"
    else
        # Logs not stored => summaries, crashlog and corefiles not moved, other logs deleted
        mv nisccLog00 nisccLog01 nisccLog02 nisccLog03 nisccLog04 nisccLog05 nisccLog06 nisccLog07 nisccLog08 nisccLog09 nisccLog10 nisccLog11 nisccLog12 TRASH/
        mv niscc_smime/p7m-ed-m-results.txt niscc_smime/p7m-sd-dt-results.txt niscc_smime/p7m-sd-op-results.txt TRASH/
    fi
    mv envDB sigDB niscc_smime niscc_ssl TRASH/
    mv CA.p12 Client.p12 client_crt.p12 server_crt.p12 TRASH/
    mv p7m-ed-m-files.txt p7m-sd-dt-files.txt p7m-sd-op-files.txt password-is-testtest1.txt detached.txt TRASH/
    mv crashme.c crashme TRASH/
}

################################################################################
# Main
################################################################################
process_args $*
create_environment
hg_pull
build_NSS
init
niscc_smime
niscc_ssl_init
force_crash
ssl_setup_dirs_simple
    ssl_simple_client_auth
    ssl_simple_server_auth
    ssl_simple_rootca
ssl_setup_dirs_resigned
    ssl_resigned_client_auth
    ssl_resigned_server_auth
    ssl_resigned_rootca
# no idea what these commented-out lines are supposed to be!
#ssl_setup_dirs_update
#    ssl_update_server_auth der
#    ssl_update_client_auth der
#    ssl_update_server_auth resigned-der
#    ssl_update_client_auth resigned-der
log_summary
mail_testLog
core_process
move_files
exit $SIZE
