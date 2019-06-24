#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# A Bourne shell script for running the NIST RNG Validation Suite
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.
BASEDIR=${1-.}
TESTDIR=${BASEDIR}/IKE
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp

ike_requests="
ikev1_dsa.req
ikev1_psk.req
ikev2.req
"

if [ ${COMMAND} = "verify" ]; then
    result=0
    for request in $ike_requests; do
	sh ./validate1.sh ${TESTDIR} $request
	last_result=$?
	result=`expr $result + $last_result`
    done
    exit $result
fi

request=ikev1_dsa.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ikev1 ${REQDIR}/$request > ${RSPDIR}/$response
request=ikev1_psk.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ikev1-psk ${REQDIR}/$request > ${RSPDIR}/$response
request=ikev2.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ikev2 ${REQDIR}/$request > ${RSPDIR}/$response
exit 0;
