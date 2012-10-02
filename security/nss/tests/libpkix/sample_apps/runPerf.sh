#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# runPerf.sh
#

curdir=`pwd`
cd ../common
. ./libpkix_init.sh > /dev/null
cd ${curdir}

numtests=0
passed=0
testunit=PERFORMANCE

totalErrors=0
loopErrors=0

ParseArgs $*

testHeadingEcho

Display "\nRunning executables at ${DIST_BIN}"
Display "Using libraries at ${LD_LIBRARY_PATH}"


# Check the performance data ...
perfTest()
{

    Display ""
    Display "*******************************************************************************"
    Display "START OF PKIX PERFORMANCE SCENARIOS ${memText}"
Display "*******************************************************************************"
    Display ""

    while read perfPgm args; do
        numtests=`expr ${numtests} + 1`
        Display "Running ${perfPgm} ${args}"
        if [ ${checkmem} -eq 1 ]; then
            dbx -C -c "runargs $args; check -all ;run;exit" ${DIST_BIN}/${perfPgm} > ${testOut} 2>&1
        else
            ${DIST_BIN}/${perfPgm} ${args} > ${testOut} 2>&1
        fi

        # Examine output file to see if test failed and keep track of number
        # of failures and names of failed tests. This assumes that the test
        # uses our utility library for displaying information
	
        outputCount=`cat ${testOut} | grep "per second"`

        if [ $? -ne 0 ]; then
            errors=`expr ${errors} + 1`
            failedpgms="${failedpgms}${perfPgm} ${args}\n"
            cat ${testOut}
        else
            Display ${outputCount}
            passed=`expr ${passed} + 1`
        fi

        if [ ${checkmem} -eq 1 ]; then
            grep "(actual leaks:" ${testOut} > ${testOutMem} 2>&1
            if [ $? -ne 0 ]; then
                prematureErrors=`expr ${prematureErrors} + 1`
                failedprematurepgms="${failedprematurepgms}${perfPgm} "
                Display "...program terminated prematurely (unable to check for memory leak errors) ..."
            else
                grep  "(actual leaks:         1  total size:       4 bytes)" ${testOut} > /dev/null 2>&1
                if [ $? -ne 0 ]; then
                    memErrors=`expr ${memErrors} + 1`
                    failedmempgms="${failedmempgms}${perfPgm} "
                    Display ${testOutMem}
                fi
            fi
        fi
    done
    return ${errors}
}


# If there is race condition bug, may this test catch it...
loopTest()
{
    totalLoop=10

    Display ""
    Display "*******************************************************************************"
    Display "START OF TESTS FOR PKIX PERFORMANCE SANITY LOOP (${totalLoop} times)"
Display "*******************************************************************************"
    Display ""

    errors=0
    iLoop=0
    perfPgm="${DIST_BIN}/pkixutil libpkix_buildthreads -d . 5 8 ValidCertificatePathTest1EE"

    while [ $iLoop -lt $totalLoop ]
    do
        iLoop=`expr $iLoop + 1`
        numtests=`expr ${numtests} + 1`

        Display "Running ${perfPgm}"
        ${perfPgm} > ${testOut} 2>&1
        Display `cat ${testOut} | grep "per second"`

        outputCount=`cat ${testOut} | grep "per second"`

        if [ $? -ne 0 ]; then
            errors=`expr ${errors} + 1`
            failedpgms="${failedpgms} ${perfPgm}\n"
            cat ${testOut}
        else
            passed=`expr ${passed} + 1`
        fi
    done

    return ${errors}

}

#main
perfTest <<EOF
pkixutil libpkix_buildthreads -d . 5 1 ValidCertificatePathTest1EE
pkixutil libpkix_buildthreads -d . 5 8 ValidCertificatePathTest1EE
pkixutil nss_threads -d . 5 1 ValidCertificatePathTest1EE
pkixutil nss_threads -d . 5 8 ValidCertificatePathTest1EE
EOF

totalErrors=$?
html_msg ${totalErrors} 0 "&nbsp;&nbsp;&nbsp;performance test: passed ${passed} of ${numtests} tests"

numtests=0
passed=0
loopTest
loopErrors=$?
totalErrors=`expr ${totalErrors} + ${loopErrors}`
html_msg ${totalErrors} 0 "&nbsp;&nbsp;&nbsp;loop test: passed ${passed} of ${numtests} tests"

testEndingEcho

exit ${totalErrors}
