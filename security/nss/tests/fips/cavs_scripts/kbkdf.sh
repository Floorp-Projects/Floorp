#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# A Bourne shell script for running the NIST SHA Algorithm Validation Suite
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.
BASEDIR=${1-.}
TESTDIR=${BASEDIR}/KBKDF
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp

all_requests="
KBKDFCounter.req
"

if [ ${COMMAND} = "verify" ]; then
    result=0
    for request in $all_requests; do
        sh ./validate1.sh ${TESTDIR} $request
        last_result=$?
        result=`expr $result + $last_result`
    done
    exit $result
fi

test -d "${RSPDIR}" || mkdir "${RSPDIR}"

for request in $all_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest kbkdf ${REQDIR}/$request > ${RSPDIR}/$response
done
exit 0
