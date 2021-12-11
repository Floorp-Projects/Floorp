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
    result=0
    # ecdh init
    name=KASFunctionTest_ECCEphemeralUnified_NOKC_ZZOnly_init
    echo ">>>>>  $name"
    fipstest ecdh init-verify ${RSPDIR}/$name.rsp | grep ^Result.=.F
    test 1 = $?
    last_result=$?
    result=`expr $result + $last_result`
    # ecdh response
    name=KASFunctionTest_ECCEphemeralUnified_NOKC_ZZOnly_resp
    echo ">>>>>  $name"
    fipstest ecdh resp-verify ${RSPDIR}/$name.rsp | grep ^Result.=.F
    test 1 = $?
    last_result=$?
    result=`expr $result + $last_result`
    # ecdh init verify
    sh ./validate1.sh ${TESTDIR} KASValidityTest_ECCEphemeralUnified_NOKC_ZZOnly_init.req ' ' '-e /^Result.=.F/s;.(.*);; -e /^Result.=.P/s;.(.*);;'
    last_result=$?
    result=`expr $result + $last_result`
    # ecdh response verify
    sh ./validate1.sh ${TESTDIR} KASValidityTest_ECCEphemeralUnified_NOKC_ZZOnly_resp.req ' ' '-e /^Result.=.F/s;.(.*);; -e /^Result.=.P/s;.(.*);;'
    last_result=$?
    result=`expr $result + $last_result`
    # dh init
    name=KASFunctionTest_FFCEphem_NOKC_ZZOnly_init
    echo ">>>>>  $name"
    fipstest dh init-verify ${RSPDIR}/$name.rsp | grep ^Result.=.F
    test 1 = $?
    last_result=$?
    result=`expr $result + $last_result`
    # dh response
    name=KASFunctionTest_FFCEphem_NOKC_ZZOnly_resp
    echo ">>>>>  $name"
    fipstest dh resp-verify ${RSPDIR}/$name.rsp | grep ^Result.=.F
    test 1 = $?
    last_result=$?
    result=`expr $result + $last_result`
    # ecdh init verify
    sh ./validate1.sh ${TESTDIR} KASValidityTest_FFCEphem_NOKC_ZZOnly_init.req ' ' '-e /^Result.=.F/s;.(.*);; -e /^Result.=.P/s;.(.*);;'
    last_result=$?
    result=`expr $result + $last_result`
    # ecdh response verify
    sh ./validate1.sh ${TESTDIR} KASValidityTest_FFCEphem_NOKC_ZZOnly_resp.req ' ' '-e /^Result.=.F/s;.(.*);; -e /^Result.=.P/s;.(.*);;'
    last_result=$?
    result=`expr $result + $last_result`
    exit $result
fi

test -d "${RSPDIR}" || mkdir "${RSPDIR}"

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
exit 0
