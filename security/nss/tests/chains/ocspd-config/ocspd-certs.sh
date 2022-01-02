#!/bin/bash

DATA_DIR=$1
OCSP_DIR=$2
CERT_DIR=$3

TEST_PWD="nssnss"
CONF_TEMPLATE="ocspd.conf.template"

convert_cert()
{
    CERT_NAME=$1
    CERT_SIGNER=$2

    openssl x509 -in ${DATA_DIR}/${CERT_NAME}${CERT_SIGNER}.der -inform DER -out ${DATA_DIR}/${CERT_NAME}.pem -outform PEM
}

convert_crl()
{
    CRL_NAME=$1

    openssl crl -in ${DATA_DIR}/${CRL_NAME}.crl -inform DER -out ${DATA_DIR}/${CRL_NAME}crl.pem -outform PEM
}

convert_key()
{
    KEY_NAME=$1

    pk12util -o ${DATA_DIR}/${KEY_NAME}.p12 -n ${KEY_NAME} -d ${DATA_DIR}/${KEY_NAME}DB -k ${DATA_DIR}/${KEY_NAME}DB/dbpasswd -W ${TEST_PWD}
    openssl pkcs12 -in ${DATA_DIR}/${KEY_NAME}.p12 -out ${DATA_DIR}/${KEY_NAME}.key.tmp -passin pass:${TEST_PWD} -passout pass:${TEST_PWD}

    STATUS=0
    cat ${DATA_DIR}/${KEY_NAME}.key.tmp | while read LINE; do
        echo "${LINE}" | grep "BEGIN ENCRYPTED PRIVATE KEY" > /dev/null && STATUS=1
        [ ${STATUS} -eq 1 ] && echo "${LINE}"
        echo "${LINE}" | grep "END ENCRYPTED PRIVATE KEY" > /dev/null && break
    done > ${DATA_DIR}/${KEY_NAME}.key
    
    rm ${DATA_DIR}/${KEY_NAME}.key.tmp
}

create_conf()
{
    CONF_FILE=$1
    CA=$2
    OCSP=$3
    PORT=$4 

    cat ${CONF_TEMPLATE} | \
        sed "s:@DIR@:${OCSP_DIR}:" | \
        sed "s:@CA_CERT@:${DATA_DIR}/${CA}.pem:" | \
        sed "s:@CA_CRL@:${DATA_DIR}/${CA}crl.pem:" | \
        sed "s:@CA_KEY@:${DATA_DIR}/${CA}.key:" | \
        sed "s:@OCSP_PID@:${OCSP}.pid:" | \
        sed "s:@PORT@:${PORT}:" \
        > ${CONF_FILE}
}

copy_cert()
{
    CERT_NAME=$1
    CERT_SIGNER=$2

    cp ${DATA_DIR}/${CERT_NAME}${CERT_SIGNER}.der ${CERT_DIR}/${CERT_NAME}.cert
}


copy_key()
{
    KEY_NAME=$1

    cp ${DATA_DIR}/${KEY_NAME}.p12 ${CERT_DIR}/${KEY_NAME}.p12
}

convert_cert OCSPRoot
convert_crl OCSPRoot
convert_key OCSPRoot

convert_cert OCSPCA1 OCSPRoot
convert_crl OCSPCA1
convert_key OCSPCA1

convert_cert OCSPCA2 OCSPRoot
convert_crl OCSPCA2
convert_key OCSPCA2

convert_cert OCSPCA3 OCSPRoot
convert_crl OCSPCA3
convert_key OCSPCA3

create_conf ocspd0.conf OCSPRoot ocspd0 2600
create_conf ocspd1.conf OCSPCA1 ocspd1 2601
create_conf ocspd2.conf OCSPCA2 ocspd2 2602
create_conf ocspd3.conf OCSPCA3 ocspd3 2603

copy_cert OCSPRoot
copy_cert OCSPCA1 OCSPRoot
copy_cert OCSPCA2 OCSPRoot
copy_cert OCSPCA3 OCSPRoot
copy_cert OCSPEE11 OCSPCA1
copy_cert OCSPEE12 OCSPCA1
copy_cert OCSPEE13 OCSPCA1
copy_cert OCSPEE14 OCSPCA1
copy_cert OCSPEE15 OCSPCA1
copy_cert OCSPEE21 OCSPCA2
copy_cert OCSPEE22 OCSPCA2
copy_cert OCSPEE23 OCSPCA2
copy_cert OCSPEE31 OCSPCA3
copy_cert OCSPEE32 OCSPCA3
copy_cert OCSPEE33 OCSPCA3

copy_key OCSPRoot
copy_key OCSPCA1
copy_key OCSPCA2
copy_key OCSPCA3

