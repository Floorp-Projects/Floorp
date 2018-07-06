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
TESTDIR=${BASEDIR}/KAS
COMMAND=${2-run}
REQDIR=${TESTDIR}/req
RSPDIR=${TESTDIR}/resp


#
if [ ${COMMAND} = "verify" ]; then
#
# need verify for KAS tests

# verify generated keys
#    name=KeyPair
#    echo ">>>>>  $name"
#    fipstest dsa keyver ${RSPDIR}/$name.rsp | grep ^Result.=.F
# verify generated pqg values
#    name=PQGGen
#    echo ">>>>>  $name"
#    fipstest dsa pqgver ${RSPDIR}/$name.rsp | grep ^Result.=.F
# verify PQGVer with known answer
#    sh ./validate1.sh ${TESTDIR} PQGVer.req ' ' '-e /^Result.=.F/s;.(.*);; -e /^Result.=.P/s;.(.*);;'
# verify signatures
#    name=SigGen
#    echo ">>>>>  $name"
#    fipstest dsa sigver ${RSPDIR}/$name.rsp | grep ^Result.=.F
# verify SigVer with known answer
#    sh ./validate1.sh ${TESTDIR} SigVer.req ' ' '-e /^X.=/d -e /^Result.=.F/s;.(.*);;'
    exit 0
fi

request=KASFunctionTest_ECCEphemeralUnified_NOKC_ZZOnly_init.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ecdh init-func ${REQDIR}/$request > ${RSPDIR}/$response

request=KASFunctionTest_ECCEphemeralUnified_NOKC_ZZOnly_resp.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ecdh resp-func ${REQDIR}/$request > ${RSPDIR}/$response

request=KASValidityTest_ECCEphemeralUnified_NOKC_ZZOnly_init.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ecdh init-verify ${REQDIR}/$request > ${RSPDIR}/$response

request=KASValidityTest_ECCEphemeralUnified_NOKC_ZZOnly_resp.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest ecdh resp-verify ${REQDIR}/$request > ${RSPDIR}/$response

request=KASFunctionTest_FFCEphem_NOKC_ZZOnly_init.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dh init-func ${REQDIR}/$request > ${RSPDIR}/$response

request=KASFunctionTest_FFCEphem_NOKC_ZZOnly_resp.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dh resp-func ${REQDIR}/$request > ${RSPDIR}/$response

request=KASValidityTest_FFCEphem_NOKC_ZZOnly_init.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dh init-verify ${REQDIR}/$request > ${RSPDIR}/$response

request=KASValidityTest_FFCEphem_NOKC_ZZOnly_resp.req
response=`echo $request | sed -e "s/req/rsp/"`
echo $request $response
fipstest dh resp-verify ${REQDIR}/$request > ${RSPDIR}/$response

