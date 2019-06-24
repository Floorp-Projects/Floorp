#!/bin/sh
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# A Bourne shell script for running the NIST DSA Validation System
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.
BASEDIR=${1-.}
TESTDIR=${BASEDIR}/DSA2
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp


#
# several of the DSA tests do use known answer tests to verify the result.
# in those cases, feed generated tests back into the fipstest tool and
# see if we can verify those value. NOTE: th PQGVer and SigVer tests verify
# the dsa pqgver and dsa sigver functions, so we know they can detect errors
# in those PQGGen and SigGen. Only the KeyPair verify is potentially circular.
#
if [ ${COMMAND} = "verify" ]; then
    result=0
# verify generated keys
    name=KeyPair
    echo ">>>>>  $name"
    fipstest dsa keyver ${RSPDIR}/$name.rsp | grep ^Result.=.F
    test 1 = $?
    last_result=$?
    result=`expr $result + $last_result`
# verify generated pqg values
    name=PQGGen
    echo ">>>>>  $name"
    fipstest dsa pqgver ${RSPDIR}/$name.rsp | grep ^Result.=.F
    test 1 = $?
    last_result=$?
    result=`expr $result + $last_result`
# verify PQGVer with known answer
    sh ./validate1.sh ${TESTDIR} PQGVer1863.req ' ' '-e /^Result.=.F/s;.(.*);; -e /^Result.=.P/s;.(.*);;'
    last_result=$?
    result=`expr $result + $last_result`
# verify signatures
    name=SigGen
    echo ">>>>>  $name"
    fipstest dsa sigver ${RSPDIR}/$name.rsp | grep ^Result.=.F
    test 1 = $?
    last_result=$?
    result=`expr $result + $last_result`
# verify SigVer with known answer
    sh ./validate1.sh ${TESTDIR} SigVer.req ' ' '-e /^X.=/d -e /^Result.=.F/s;.(.*);;'
    last_result=$?
    result=`expr $result + $last_result`
    exit $result
fi

request=KeyPair.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dsa keypair ${REQDIR}/$request > ${RSPDIR}/$response

request=PQGGen.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dsa pqggen ${REQDIR}/$request > ${RSPDIR}/$response

request=PQGVer1863.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dsa pqgver ${REQDIR}/$request > ${RSPDIR}/$response

request=SigGen.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dsa siggen ${REQDIR}/$request > ${RSPDIR}/$response

request=SigVer.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dsa sigver ${REQDIR}/$request > ${RSPDIR}/$response
exit 0
