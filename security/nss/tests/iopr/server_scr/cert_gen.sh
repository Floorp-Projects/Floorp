#!/bin/sh    

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

signCert() {
    dir=$1
    crtDir=$2
    crtName=$3
    crtSN=$4
    req=$5
    cuAddParam=$6

    repAndExec \
        certutil $cuAddParam -C -c "TestCA" -m $crtSN -v 599 -d "${dir}" \
             -i $req -o "$crtDir/${crtName}.crt" -f "${PW_FILE}" 2>&1
    return $RET
}

createSignedCert() {
    dir=$1
    certName=$2
    certSN=$3
    certSubj=$4
    keyType=$5
    exportFile=$6

    echo Creating cert $certName with SN=$certSN

    CU_SUBJECT="$certSubj"
    repAndExec \
        certutil -R -d $dir -f "${PW_FILE}" -z "${NOISE_FILE}" \
                  -k $keyType -o $dir/req  2>&1
    [ "$RET" -ne 0 ] && return $RET

    signCert $dir $dir $certName $certSN $dir/req
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    rm -f $dir/req

    repAndExec \
        certutil -A -n ${certName}-${keyType} -t "u,u,u" -d "${dir}" -f "${PW_FILE}" \
                    -i "$dir/${certName}.crt" 2>&1
    [ "$RET" -ne 0 ] && return $RET

    repAndExec \
        pk12util -d $dir -o $exportFile -n ${certName}-${keyType} -k ${PW_FILE} -W iopr
    [ "$RET" -ne 0 ] && return $RET
    return 0
}

generateServerCerts() {
    certDir=$1
    serverName=$2
    servCertReq=$3
    
    [ -z "$certDir" ] && echo "Cert directory should not be empty" && exit 1
    [ -z "$serverName" ] && echo "Server name should not be empty" && exit 1

    mkdir -p $certDir
    [ $? -ne 0 ] && echo "Can not create dir: $certDir" && exit 1
    

    dir=/tmp/db.$$
    if [ -d "$dir" ]; then
        rm -f $dir
    fi
    mkdir -p $dir
    [ $? -ne 0 ] && echo "Can not create dir: $dir" && exit 1

    PW_FILE=$dir/nss.pwd
    NOISE_FILE=$dir/nss.noise
    echo nss > $PW_FILE

    date >> ${NOISE_FILE} 2>&1

    repAndExec \
        certutil -d $dir -N -f $PW_FILE
    [ "$RET" -ne 0 ] && return $RET
    

    certName=TestCA
    CU_SUBJECT="CN=NSS IOPR Test CA $$, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    repAndExec \
        certutil -S -n $certName -t "CTu,CTu,CTu" -v 600 -x -d ${dir} -1 -2 \
                -f ${PW_FILE} -z ${NOISE_FILE} -m 10000 2>&1 <<EOF
5
6
9
n
y
-1
n
EOF

    repAndExec \
        certutil -L -n $certName -r -d ${dir} -o $certDir/$certName.crt 
    [ "$RET" -ne 0 ] && return $RET

    repAndExec \
        pk12util -d $dir -o $certDir/$certName.p12 -n $certName -k ${PW_FILE} -W iopr
    [ "$RET" -ne 0 ] && return $RET

    if [ "$servCertReq" -a -f $servCertReq ]; then
        grep REQUEST $servCertReq >/dev/null 2>&1
        signCert $dir $certDir ${serverName}_ext 501 $servCertReq `test $? -eq 0 && echo -a`
        ret=$?
        [ "$ret" -ne 0 ] && return $ret
    fi

    certName=$serverName
    certSubj="CN=$certName, E=${certName}-rsa@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    createSignedCert $dir $certName 500 "$certSubj" rsa $certDir/${certName}-rsa.p12
    ret=$?
    [ "$ret" -ne 0 ] && return $ret
    certName=$serverName

    certName=$serverName
    certSubj="CN=$certName, E=${certName}-dsa@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    createSignedCert $dir $certName 501 "$certSubj" dsa $certDir/${certName}-dsa.p12
    ret=$?
    [ "$ret" -ne 0 ] && return $ret
   
    certName=TestUser510
    certSubj="CN=$certName, E=${certName}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    createSignedCert $dir $certName 510 "$certSubj" rsa $certDir/${certName}-rsa.p12
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=TestUser511
    certSubj="CN=$certName, E=${certName}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    createSignedCert $dir $certName 511 "$certSubj" dsa $certDir/${certName}-dsa.p12
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=TestUser512
    certSubj="CN=$certName, E=${certName}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    createSignedCert $dir $certName 512 "$certSubj" rsa $certDir/${certName}-rsa.p12
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    certName=TestUser513
    certSubj="CN=$certName, E=${certName}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    createSignedCert $dir $certName 513 "$certSubj" dsa $certDir/${certName}-dsa.p12
    ret=$?
    [ "$ret" -ne 0 ] && return $ret

    crlUpdate=`date +%Y%m%d%H%M%SZ`
    crlNextUpdate=`echo $crlUpdate | sed 's/20/21/'`
    repAndExec \
        crlutil -d $dir -G -n "TestCA" -f ${PW_FILE} -o $certDir/TestCA.crl <<EOF_CRLINI
update=$crlUpdate
nextupdate=$crlNextUpdate
addcert 509-511 $crlUpdate
EOF_CRLINI
    [ "$RET" -ne 0 ] && return $RET

    rm -rf $dir
    return 0
}


if [ -z "$1" -o -z "$2" ]; then
    echo "$0 <dest dir> <cert name> [cert req]"
    exit 1
fi
generateServerCerts $1 $2 $3
exit $?
