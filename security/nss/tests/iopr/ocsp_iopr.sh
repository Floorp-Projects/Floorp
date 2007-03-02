#! /bin/sh
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
#
# FIXME - Netscape - NSS
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
            html_failed "<TR><TD>Incorrect test params" 
            return 1
        fi
        clntParam="-l $respUrl -t $defRespCert"
    fi

    outFile=$dbDir/ocsptest.out.$$
    ocspclnt -d $dbDir -S $cert $clntParam &> $outFile
    res=$?
    echo "ocspclnt output:"
    cat $outFile
    [ -z "`grep succeeded $outFile`" ] && res=1
    
    rm -f $outFile
    return $res
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
    
    html_head "OCSP testing with responder at $IOPR_HOSTADDR. <br>" \
        "Test Type: $testDescription"

    if [ -n "$testResponder" ]; then
        responderUrl="$testProto://$servName:$testPort"
    else
        responderUrl=""
    fi

    for certName in $testValidCertNames; do
        ocsp_get_cert_status $dbDir $certName "$responderUrl" "$testResponder"
        html_msg $? 0 "Getting status of a valid cert ($certName)" \
            "produced a returncode of $ret, expected is $value"
    done

    for certName in $testRevokedCertNames; do
        ocsp_get_cert_status $dbDir $certName "$responderUrl" "$testResponder"
        html_msg $? 1 "Getting status of a unvalid cert ($certName)" \
            "produced a returncode of $ret, expected is $value" 
    done

    for certName in $testStatUnknownCertNames; do
        ocsp_get_cert_status $dbDir $certName "$responderUrl" "$testResponder"
        html_msg $? 1 "Getting status of a cert with unknown status ($certName)" \
                    "produced a returncode of $ret, expected is $value"
    done
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
        echo "Testing ocsp interoperability.
                Client: local(tstclnt).
                Responder: remote($IOPR_HOSTADDR)"
        
        for ocspTestType in ${supportedTests_new}; do
            if [ -z "`echo $ocspTestType | grep -i ocsp`" ]; then
                continue
            fi
            ocsp_iopr $ocspTestType ${IOPR_HOSTADDR} \
                ${IOPR_OCSP_CLIENTDIR}_${IOPR_HOSTADDR}
        done
        echo "================================================"
        echo "Done testing ocsp interoperability with $IOPR_HOSTADDR"
    done
    NO_ECC_CERTS=0
    return 0
}

