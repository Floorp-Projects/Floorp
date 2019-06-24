#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# A Bourne shell script for running the NIST tdea Algorithm Validation Suite
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.

BASEDIR=${1-.}
TESTDIR=${BASEDIR}/TDES
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp

#CBC_Known_Answer_tests
#Initial Permutation KAT
#Permutation Operation KAT
#Subsitution Table KAT
#Variable Key KAT
#Variable PlainText KAT
cbc_kat_requests="
TCBCinvperm.req
TCBCpermop.req
TCBCsubtab.req
TCBCvarkey.req
TCBCvartext.req
"

#CBC Monte Carlo KATs
cbc_monte_requests="
TCBCMonte1.req
TCBCMonte2.req
TCBCMonte3.req
"
#Multi-block Message KATs
cbc_mmt_requests="
TCBCMMT1.req
TCBCMMT2.req
TCBCMMT3.req
"

ecb_kat_requests="
TECBinvperm.req
TECBpermop.req
TECBsubtab.req
TECBvarkey.req
TECBvartext.req
"

ecb_monte_requests="
TECBMonte1.req
TECBMonte2.req
TECBMonte3.req
"

ecb_mmt_requests="
TECBMMT1.req
TECBMMT2.req
TECBMMT3.req
"


if [ ${COMMAND} = "verify" ]; then
    result=0
    for request in $cbc_kat_requests $cbc_monte_requests $cbc_mmt_requests $ecb_kat_requests $ecb_monte_requests $ecb_mmt_requests
    do
	sh ./validate1.sh ${TESTDIR} $request "-e /^NumKeys/d"
	last_result=$?
	result=`expr $result + $last_result`
    done
    exit $result
fi

for request in $cbc_kat_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest tdea kat cbc ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $cbc_mmt_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest tdea mmt cbc ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $cbc_monte_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest tdea mct cbc ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $ecb_kat_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest tdea kat ecb ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $ecb_mmt_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest tdea mmt ecb ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $ecb_monte_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest tdea mct ecb ${REQDIR}/$request > ${RSPDIR}/$response
done
exit 0
