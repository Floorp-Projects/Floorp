#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#
# A Bourne shell script for running the NIST AES Algorithm Validation Suite
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.

BASEDIR=${1-.}
TESTDIR=${BASEDIR}/AES
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp

cbc_kat_requests="
CBCGFSbox128.req
CBCGFSbox192.req
CBCGFSbox256.req
CBCKeySbox128.req
CBCKeySbox192.req
CBCKeySbox256.req
CBCVarKey128.req
CBCVarKey192.req
CBCVarKey256.req
CBCVarTxt128.req
CBCVarTxt192.req
CBCVarTxt256.req
"

cbc_mct_requests="
CBCMCT128.req
CBCMCT192.req
CBCMCT256.req
"

cbc_mmt_requests="
CBCMMT128.req
CBCMMT192.req
CBCMMT256.req
"

ecb_kat_requests="
ECBGFSbox128.req
ECBGFSbox192.req
ECBGFSbox256.req
ECBKeySbox128.req
ECBKeySbox192.req
ECBKeySbox256.req
ECBVarKey128.req
ECBVarKey192.req
ECBVarKey256.req
ECBVarTxt128.req
ECBVarTxt192.req
ECBVarTxt256.req
"

ecb_mct_requests="
ECBMCT128.req
ECBMCT192.req
ECBMCT256.req
"

ecb_mmt_requests="
ECBMMT128.req
ECBMMT192.req
ECBMMT256.req
"

if [ ${COMMAND} = "verify" ]; then
    result=0;
    for request in $cbc_kat_requests $cbc_mct_requests $cbc_mmt_requests $ecb_kat_requests $ecb_mct_requests $ecb_mmt_requests; do
	sh ./validate1.sh ${TESTDIR} $request
	last_result=$?
	result=`expr $result + $last_result`
    done
    exit $result
fi

test -d "${RSPDIR}" || mkdir "${RSPDIR}"

for request in $cbc_kat_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes kat cbc ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $cbc_mct_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes mct cbc ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $cbc_mmt_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes mmt cbc ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $ecb_kat_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes kat ecb ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $ecb_mct_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes mct ecb ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $ecb_mmt_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes mmt ecb ${REQDIR}/$request > ${RSPDIR}/$response
done
exit 0
