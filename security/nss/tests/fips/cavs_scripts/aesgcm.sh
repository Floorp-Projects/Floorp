#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# A Bourne shell script for running the NIST AES Algorithm Validation Suite
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.

BASEDIR=${1-.}
TESTDIR=${BASEDIR}/AES_GCM
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp

gcm_decrypt_requests="
gcmDecrypt128.req
gcmDecrypt192.req
gcmDecrypt256.req
"

gcm_encrypt_extiv_requests="
gcmEncryptExtIV128.req
gcmEncryptExtIV192.req
gcmEncryptExtIV256.req
"
gcm_encrypt_intiv_requests="
"

#gcm_encrypt_intiv_requests="
#gcmEncryptIntIV128.req
#gcmEncryptIntIV192.req
#gcmEncryptIntIV256.req
#"

if [ ${COMMAND} = "verify" ]; then
    result=0
    for request in $gcm_decrypt_requests $gcm_encrypt_extiv_requests; do
	sh ./validate1.sh ${TESTDIR} $request ' ' '-e /Reason:/d'
	last_result=$?
	result=`expr $result + $last_result`
    done
    for request in $gcm_encrypt_intiv_requests; do
	name=`basename $request .req`
    	echo ">>>>>  $name"
        fipstest aes gcm decrypt ${RSPDIR}/$name.rsp | grep FAIL
	test 1 = $?
	last_result=$?
	result=`expr $result + $last_result`
    done
    exit $result
fi

test -d "${RSPDIR}" || mkdir "${RSPDIR}"

for request in $gcm_decrypt_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes gcm decrypt ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $gcm_encrypt_intiv_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes gcm encrypt_intiv ${REQDIR}/$request > ${RSPDIR}/$response
done
for request in $gcm_encrypt_extiv_requests; do
    response=`echo $request | sed -e "s/req/rsp/"`
    echo $request $response
    fipstest aes gcm encrypt_extiv ${REQDIR}/$request > ${RSPDIR}/$response
done
exit 0
