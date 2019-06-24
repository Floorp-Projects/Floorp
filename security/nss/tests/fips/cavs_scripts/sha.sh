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
TESTDIR=${BASEDIR}/SHA
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp

sha_ShortMsg_requests="
SHA1ShortMsg.req
SHA224ShortMsg.req
SHA256ShortMsg.req
SHA384ShortMsg.req
SHA512ShortMsg.req
"

sha_LongMsg_requests="
SHA1LongMsg.req
SHA224LongMsg.req
SHA256LongMsg.req
SHA384LongMsg.req
SHA512LongMsg.req
"

sha_Monte_requests="
SHA1Monte.req
SHA224Monte.req
SHA256Monte.req
SHA384Monte.req
SHA512Monte.req
"

if [ ${COMMAND} = "verify" ]; then
    result=0
    for request in $sha_ShortMsg_requests $sha_LongMsg_requests $sha_Monte_requests; do
	sh ./validate1.sh ${TESTDIR} $request
	last_result=$?
	result=`expr $result + $last_result`
    done
    exit $result
fi

for request in $sha_ShortMsg_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest sha ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $sha_LongMsg_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest sha ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $sha_Monte_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest sha ${REQDIR}/$request > ${RSPDIR}/$response
done
exit 0
