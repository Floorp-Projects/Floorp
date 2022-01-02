#!/bin/bash    

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

######################################################################################
# Server and client certs and crl generator functions. Generated files placed in a <dir>
# directory to be accessible through http://<webserver>/iopr/TestCA.crt directory.
# This functions is used for manual webserver configuration and it is not a part of
# nss test run.
# To create certs use the following command:
#       sh cert_iopr.sh cert_gen <dir> <cert name> [cert req]
# Where:
#       dir - directory where to place created files
#       cert name - name of created server cert(FQDN)
#       cert req  - cert request to be used for cert generation.
#
repAndExec() {
    echo
    if [ "$1" = "certutil" -a "$2" = "-R" -o "$2" = "-S" ]; then
        shift
        echo certutil -s "$CU_SUBJECT" $@
        certutil -s "$CU_SUBJECT" $@
        RET=$?
    else
        echo $@
        $@
        RET=$?
    fi

    return $RET
}

setExtData() {
    extData=$1

    fldNum=0
    extData=`echo $extData | sed 's/,/ /g'`
    for extDT in $extData; do
        if [ $fldNum -eq 0 ]; then
            eval extType=$extDT
            fldNum=1
            continue
        fi
        eval data${fldNum}=$extDT
        fldNum=`expr $fldNum + 1`
    done
}

signCert() {
    dir=$1
    crtDir=$2
    crtName=$3
    crtSN=$4
    req=$5
    cuAddParam=$6
    extList=$7

    if [ -z "$certSigner" ]; then
        certSigner=TestCA
    fi

    extCmdLine=""
    extCmdFile=$dir/extInFile; rm -f $extCmdFile
    touch $extCmdFile
    extList=`echo $extList | sed 's/;/ /g'`
    for ext in $extList; do
        setExtData $ext
        [ -z "$extType" ] && echo "incorrect extention format" && return 1
        case $extType in
        ocspDR)
                extCmdLine="$extCmdLine -6"
                cat <<EOF >> $extCmdFile
5
9
y
EOF
                break
                exit 1
                ;;
        AIA)    
                extCmdLine="$extCmdLine -9"
                cat <<EOF >> $extCmdFile
2
7
$data1
0
n
n
EOF
                break
                ;;
            *)
                echo "Unsupported extension type: $extType"
                break
                ;;
        esac
    done
    echo "cmdLine: $extCmdLine"
    echo "cmdFile: "`cat $extCmdFile`
    repAndExec \
        certutil $cuAddParam -C -c $certSigner -m $crtSN -v 599 -d "${dir}" \
        -i $req -o "$crtDir/${crtName}.crt" -f "${PW_FILE}" $extCmdLine <$extCmdFile 2>&1
    return $RET
}

createSignedCert() {
    dir=$1
    certDir=$2
    certName=$3
    certSN=$4
    certSubj=$5
    keyType=$6
    extList=$7

    echo Creating cert $certName-$keyType with SN=$certSN

    CU_SUBJECT="CN=$certName, E=${certName}-${keyType}@example.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    repAndExec \
        certutil -R -d $dir -f "${PW_FILE}" -z "${NOISE_FILE}" \
                  -k $keyType -o $dir/req  2>&1
    [ "$RET" -ne 0 ] && return $RET

    signCert $dir $dir $certName-$keyType $certSN $dir/req "" $extList
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    rm -f $dir/req

    repAndExec \
        certutil -A -n ${certName}-$keyType -t "u,u,u" -d "${dir}" -f "${PW_FILE}" \
                    -i "$dir/${certName}-$keyType.crt" 2>&1
    [ "$RET" -ne 0 ] && return $RET

    cp "$dir/${certName}-$keyType.crt" $certDir

    repAndExec \
        pk12util -d $dir -o $certDir/$certName-$keyType.p12 -n ${certName}-$keyType \
                     -k ${PW_FILE} -W iopr
    [ "$RET" -ne 0 ] && return $RET
    return 0
}

generateAndExportSSLCerts() {
    dir=$1
    certDir=$2
    serverName=$3
    servCertReq=$4

    if [ "$servCertReq" -a -f $servCertReq ]; then
        grep REQUEST $servCertReq >/dev/null 2>&1
        signCert $dir $certDir ${serverName}_ext 501 $servCertReq `test $? -eq 0 && echo -a`
        ret=$?
        [ "$ret" -ne 0 ] && return $ret
    fi

    certName=$serverName
    createSignedCert $dir $certDir $certName 500 "$certSubj" rsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    createSignedCert $dir $certDir $certName 501 "$certSubj" dsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret
   
    certName=TestUser510
    createSignedCert $dir $certDir $certName 510 "$certSubj" rsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=TestUser511
    createSignedCert $dir $certDir $certName 511 "$certSubj" dsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=TestUser512
    createSignedCert $dir $certDir $certName 512 "$certSubj" rsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=TestUser513
    createSignedCert $dir $certDir $certName 513 "$certSubj" dsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret
}

generateAndExportOCSPCerts() {
    dir=$1
    certDir=$2

    certName=ocspTrustedResponder
    createSignedCert $dir $certDir $certName 525 "$certSubj" rsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspDesignatedResponder
    createSignedCert $dir $certDir $certName 526 "$certSubj" rsa ocspDR
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspTRTestUser514
    createSignedCert $dir $certDir $certName 514 "$certSubj" rsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspTRTestUser516
    createSignedCert $dir $certDir $certName 516 "$certSubj" rsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspRCATestUser518
    createSignedCert $dir $certDir $certName 518 "$certSubj" rsa \
        AIA,http://dochinups.red.iplanet.com:2561
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspRCATestUser520
    createSignedCert $dir $certDir $certName 520 "$certSubj" rsa \
        AIA,http://dochinups.red.iplanet.com:2561
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspDRTestUser522
    createSignedCert $dir $certDir $certName 522 "$certSubj" rsa \
        AIA,http://dochinups.red.iplanet.com:2562
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspDRTestUser524
    createSignedCert $dir $certDir $certName 524 "$certSubj" rsa \
        AIA,http://dochinups.red.iplanet.com:2562
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    generateAndExportCACert $dir "" TestCA-unknown
    [ $? -ne 0 ] && return $ret
    
    certSigner=TestCA-unknown
    
    certName=ocspTRUnkownIssuerCert
    createSignedCert $dir $certDir $certName 531 "$certSubj" rsa
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspRCAUnkownIssuerCert
    createSignedCert $dir $certDir $certName 532 "$certSubj" rsa \
        AIA,http://dochinups.red.iplanet.com:2561
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=ocspDRUnkownIssuerCert
    createSignedCert $dir $certDir $certName 533 "$certSubj" rsa \
        AIA,http://dochinups.red.iplanet.com:2562
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certSigner=""
    
    return 0
}

generateAndExportCACert() {
    dir=$1
    certDirL=$2
    caName=$3

    certName=TestCA
    [ "$caName" ] && certName=$caName
    CU_SUBJECT="CN=NSS IOPR Test CA $$, E=${certName}@example.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    repAndExec \
        certutil -S -n $certName -t "CTu,CTu,CTu" -v 600 -x -d ${dir} -1 -2 \
        -f ${PW_FILE} -z ${NOISE_FILE} -m `expr $$ + 2238` >&1 <<EOF
5
6
9
n
y
-1
n
EOF

    if [ "$certDirL" ]; then
        repAndExec \
            certutil -L -n $certName -r -d ${dir} -o $certDirL/$certName.crt 
        [ "$RET" -ne 0 ] && return $RET
        
        repAndExec \
            pk12util -d $dir -o $certDirL/$certName.p12 -n $certName -k ${PW_FILE} -W iopr
        [ "$RET" -ne 0 ] && return $RET
    fi
}


generateCerts() {
    certDir=$1
    serverName=$2
    reuseCACert=$3
    servCertReq=$4
    
    [ -z "$certDir" ] && echo "Cert directory should not be empty" && exit 1
    [ -z "$serverName" ] && echo "Server name should not be empty" && exit 1

    mkdir -p $certDir
    [ $? -ne 0 ] && echo "Can not create dir: $certDir" && exit 1
    

    dir=/tmp/db.$$
    if [ -z "$reuseCACert" ]; then
        if [ -d "$dir" ]; then
            rm -f $dir
        fi
   
        PW_FILE=$dir/nss.pwd
        NOISE_FILE=$dir/nss.noise

        mkdir -p $dir
        [ $? -ne 0 ] && echo "Can not create dir: $dir" && exit 1
        
        echo nss > $PW_FILE
        date >> ${NOISE_FILE} 2>&1
        
        repAndExec \
            certutil -d $dir -N -f $PW_FILE
        [ "$RET" -ne 0 ] && return $RET
        
        generateAndExportCACert $dir $certDir
        [ "$RET" -ne 0 ] && return $RET
    else
        dir=$reuseCACert
        PW_FILE=$dir/nss.pwd
        NOISE_FILE=$dir/nss.noise
        hasKey=`repAndExec certutil -d $dir -L | grep TestCA | grep CTu`
        [ -z "$hasKey" ] && echo "reuse CA cert has not priv key" && \
            return $RET;
    fi

    generateAndExportSSLCerts $dir $certDir $serverName $servCertReq
    [ "$RET" -ne 0 ] && return $RET

    generateAndExportOCSPCerts $dir $certDir
    [ "$RET" -ne 0 ] && return $RET

    crlUpdate=`date +%Y%m%d%H%M%SZ`
    crlNextUpdate=`echo $crlUpdate | sed 's/20/21/'`
    repAndExec \
        crlutil -d $dir -G -n "TestCA" -f ${PW_FILE} -o $certDir/TestCA.crl <<EOF_CRLINI
update=$crlUpdate
nextupdate=$crlNextUpdate
addcert 509-511 $crlUpdate
addcert 516 $crlUpdate
addcert 520 $crlUpdate
addcert 524 $crlUpdate
EOF_CRLINI
    [ "$RET" -ne 0 ] && return $RET

    rm -rf $dir
    return 0
}


if [ -z "$1" -o -z "$2" ]; then
    echo "$0 <dest dir> <server cert name> [reuse CA cert] [cert req]"
    exit 1
fi
generateCerts $1 $2 "$3" $4
exit $?
