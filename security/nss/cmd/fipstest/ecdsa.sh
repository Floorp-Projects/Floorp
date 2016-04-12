#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# A Bourne shell script for running the NIST ECDSA Validation System
#
# Before you run the script, set your PATH, LD_LIBRARY_PATH, ... environment
# variables appropriately so that the fipstest command and the NSPR and NSS
# shared libraries/DLLs are on the search path.  Then run this script in the
# directory where the REQUEST (.req) files reside.  The script generates the
# RESPONSE (.rsp) files in the same directory.
BASEDIR=${1-.}
TESTDIR=${BASEDIR}/ECDSA2
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp

#
# several of the ECDSA tests do not use known answer tests to verify the result.
# In those cases, feed generated tests back into the fipstest tool and
# see if we can verify those value. NOTE:  PQGVer and SigVer tests verify
# the dsa pqgver and dsa sigver functions, so we know they can detect errors
# in those PQGGen and SigGen. Only the KeyPair verify is potentially circular.
#
if [ ${COMMAND} = "verify" ]; then
# verify generated keys
    name=KeyPair
    echo ">>>>>  $name"
    fipstest ecdsa keyver ${RSPDIR}/$name.rsp | grep ^Result.=.F
    sh ./validate1.sh ${TESTDIR} PKV.req ' ' '-e /^X.=/d -e /^Result.=.F/s;.(.*);; -e /^Result.=.P/s;.(.*);;'
# verify signatures
    name=SigGen
    echo ">>>>>  $name"
    fipstest ecdsa sigver ${RSPDIR}/$name.rsp | grep ^Result.=.F
# verify SigVer with known answer
    sh ./validate1.sh ${TESTDIR} SigVer.req ' ' '-e /^X.=/d -e /^Result.=.F/s;.(.*);; -e /^Result.=.P/s;.(.*);;'
    exit 0
fi

request=KeyPair.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ecdsa keypair ${REQDIR}/$request > ${RSPDIR}/$response

request=PKV.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ecdsa pkv ${REQDIR}/$request > ${RSPDIR}/$response

request=SigGen.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ecdsa siggen ${REQDIR}/$request > ${RSPDIR}/$response

request=SigVer.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ecdsa sigver ${REQDIR}/$request > ${RSPDIR}/$response
