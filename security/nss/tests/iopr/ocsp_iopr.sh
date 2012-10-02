#! /bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

########################################################################
#
# mozilla/security/nss/tests/iopr/ocsp_iopr.sh
#
# NSS SSL interoperability QA. This file is included from ssl.sh
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
########################################################################
IOPR_OCSP_SOURCED=1

########################################################################
# The funtion works with variables defined in interoperability 
# configuration file that gets downloaded from a webserver.
# The function sets test parameters defind for a particular type
# of testing.
#
# No return value
#
setTestParam() {
    type=$1
    testParam=`eval 'echo $'${type}Param`
    testDescription=`eval 'echo $'${type}Descr`
    testProto=`eval 'echo $'${type}Proto`
    testPort=`eval 'echo $'${type}Port`
    testResponder=`eval 'echo $'${type}ResponderCert`
    testValidCertNames=`eval 'echo $'${type}ValidCertNames`
    testRevokedCertNames=`eval 'echo $'${type}RevokedCertNames`
    testStatUnknownCertNames=`eval 'echo $'${type}StatUnknownCertNames`
}

########################################################################
# The funtion checks status of a cert using ocspclnt.
# Params:
#    dbDir - nss cert db location
#    cert - cert in question
#    respUrl - responder url is available 
#    defRespCert - trusted responder cert
#
# Return values:
#    0 - test passed, 1 - otherwise.
#
ocsp_get_cert_status() {
    dbDir=$1
    cert=$2
    respUrl=$3
    defRespCert=$4
    
    if [ -n "$respUrl" -o -n "$defRespCert" ]; then
        if [ -z "$respUrl" -o -z "$defRespCert" ]; then
            html_failed "Incorrect test params" 
            return 1
        fi
        clntParam="-l $respUrl -t $defRespCert"
    fi

    if [ -z "${MEMLEAK_DBG}" ]; then
        outFile=$dbDir/ocsptest.out.$$
        echo "ocspclnt -d $dbDir -S $cert $clntParam"
        ${BINDIR}/ocspclnt -d $dbDir -S $cert $clntParam >$outFile 2>&1
        ret=$?
        echo "ocspclnt output:"
        cat $outFile
        [ -z "`grep succeeded $outFile`" ] && ret=1
    
        rm -f $outFile
        return $ret
    fi

    OCSP_ATTR="-d $dbDir -S $cert $clntParam"
    ${RUN_COMMAND_DBG} ${BINDIR}/ocspclnt ${OCSP_ATTR}
}

########################################################################
# The funtion checks status of a cert using ocspclnt.
# Params:
#    testType - type of the test based on type of used responder
#    servName - FQDM of the responder server
#    dbDir - nss cert db location
#
# No return value
#
ocsp_iopr() {
    testType=$1
    servName=$2
    dbDir=$3

    setTestParam $testType
    if [ "`echo $testParam | grep NOCOV`" != "" ]; then
        echo "SSL Cipher Coverage of WebServ($IOPR_HOSTADDR) excluded from " \
            "run by server configuration"
        return 0
    fi
    
    if [ -z "${MEMLEAK_DBG}" ]; then
        html_head "OCSP testing with responder at $IOPR_HOSTADDR. <br>" \
            "Test Type: $testDescription"
    fi

    if [ -n "$testResponder" ]; then
        responderUrl="$testProto://$servName:$testPort"
    else
        responderUrl=""
    fi

    if [ -z "${MEMLEAK_DBG}" ]; then
        for certName in $testValidCertNames; do
            ocsp_get_cert_status $dbDir $certName "$responderUrl" \
                "$testResponder"
            html_msg $? 0 "Getting status of a valid cert ($certName)" \
                "produced a returncode of $ret, expected is 0."
        done

        for certName in $testRevokedCertNames; do
            ocsp_get_cert_status $dbDir $certName "$responderUrl" \
                "$testResponder"
            html_msg $? 1 "Getting status of a unvalid cert ($certName)" \
                "produced a returncode of $ret, expected is 1." 
        done

        for certName in $testStatUnknownCertNames; do
            ocsp_get_cert_status $dbDir $certName "$responderUrl" \
                "$testResponder"
            html_msg $? 1 "Getting status of a cert with unknown status " \
                        "($certName) produced a returncode of $ret, expected is 1."
        done
    else
        for certName in $testValidCertNames $testRevokedCertNames \
            $testStatUnknownCertName; do
            ocsp_get_cert_status $dbDir $certName "$responderUrl" \
                "$testResponder" 
        done
    fi
}
  
#####################################################################
# Initial point for running ocsp test againt multiple hosts involved in
# interoperability testing. Called from nss/tests/ocsp/ocsp.sh
# It will only proceed with test run for a specific host if environment variable 
# IOPR_HOSTADDR_LIST was set, had the host name in the list
# and all needed file were successfully downloaded and installed for the host.
#
# Returns 1 if interoperability testing is off, 0 otherwise. 
#
ocsp_iopr_run() {
    NO_ECC_CERTS=1 # disable ECC for interoperability tests

    if [ "$IOPR" -ne 1 ]; then
        return 1
    fi
    cd ${CLIENTDIR}

    if [ -n "${MEMLEAK_DBG}" ]; then
        html_head "Memory leak checking - IOPR"
    fi

    num=1
    IOPR_HOST_PARAM=`echo "${IOPR_HOSTADDR_LIST} " | cut -f $num -d' '`
    while [ "$IOPR_HOST_PARAM" ]; do
        IOPR_HOSTADDR=`echo $IOPR_HOST_PARAM | cut -f 1 -d':'`
        IOPR_OPEN_PORT=`echo "$IOPR_HOST_PARAM:" | cut -f 2 -d':'`
        [ -z "$IOPR_OPEN_PORT" ] && IOPR_OPEN_PORT=443
        
        . ${IOPR_CADIR}_${IOPR_HOSTADDR}/iopr_server.cfg
        RES=$?
        
        num=`expr $num + 1`
        IOPR_HOST_PARAM=`echo "${IOPR_HOSTADDR_LIST} " | cut -f $num -d' '`

        if [ $RES -ne 0 -o X`echo "$wsFlags" | grep NOIOPR` != X ]; then
            continue
        fi
        
        #=======================================================
        # Check what server is configured to run ssl tests
        #
        [ -z "`echo ${supportedTests_new} | grep -i ocsp`" ] && continue;

        # Testing directories defined by webserver.
        if [ -n "${MEMLEAK_DBG}" ]; then
            LOGNAME=iopr-${IOPR_HOSTADDR}
            LOGFILE=${LOGDIR}/${LOGNAME}.log
        fi
       
        # Testing directories defined by webserver.
        echo "Testing ocsp interoperability.
                Client: local(tstclnt).
                Responder: remote($IOPR_HOSTADDR)"

        for ocspTestType in ${supportedTests_new}; do
            if [ -z "`echo $ocspTestType | grep -i ocsp`" ]; then
                continue
            fi
            if [ -n "${MEMLEAK_DBG}" ]; then
                ocsp_iopr $ocspTestType ${IOPR_HOSTADDR} \
                    ${IOPR_OCSP_CLIENTDIR}_${IOPR_HOSTADDR} 2>> ${LOGFILE}
            else
                ocsp_iopr $ocspTestType ${IOPR_HOSTADDR} \
                    ${IOPR_OCSP_CLIENTDIR}_${IOPR_HOSTADDR}
            fi
        done

        if [ -n "${MEMLEAK_DBG}" ]; then
            log_parse
            ret=$?
            html_msg ${ret} 0 "${LOGNAME}" \
                "produced a returncode of $ret, expected is 0"
        fi

        echo "================================================"
        echo "Done testing ocsp interoperability with $IOPR_HOSTADDR"
    done

    if [ -n "${MEMLEAK_DBG}" ]; then
        html "</TABLE><BR>"
    fi

    NO_ECC_CERTS=0
    return 0
}

