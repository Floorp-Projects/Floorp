#!/bin/bash
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
# The Original Code is the Network Security Services (NSS)
#
# The Initial Developer of the Original Code is Sun Microsystems, Inc.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Slavomir Katuscak <slavomir.katuscak@sun.com>, Sun Microsystems
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
# mozilla/security/nss/tests/cert/chains.sh
#
# Script to test certificate chains validity. 
#
# needs to work on all Unix and Windows platforms
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
########################################################################

############################# chains_init ##############################
# local shell function to initialize this script
########################################################################
chains_init()
{
    if [ -z "${CLEANUP}" ] ; then   # if nobody else is responsible for
        CLEANUP="${SCRIPTNAME}"     # cleaning this script will do it
    fi
    if [ -z "${INIT_SOURCED}" ] ; then
        cd ../common
        . ./init.sh
    fi

    SCRIPTNAME="chains.sh"

    CHAINS_DIR="${HOSTDIR}/chains"
    mkdir -p ${CHAINS_DIR}
    cd ${CHAINS_DIR}

    CHAINS_SCENARIOS="${QADIR}/chains/scenarios/scenarios"

    CERT_SN_CNT=$(date '+%m%d%H%M%S' | sed "s/^0*//")
    CERT_SN_FIX=$(expr ${CERT_SN_CNT} - 1000)

    PK7_NONCE=$CERT_SN_CNT
    SCEN_CNT=0

    AIA_FILES="${HOSTDIR}/aiafiles"

    CU_DATA=${HOSTDIR}/cu_data
    CRL_DATA=${HOSTDIR}/crl_data

    html_head "Certificate Chains Tests"
}

############################ chains_cleanup ############################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
chains_cleanup()
{
    html "</TABLE><BR>"
    cd ${QADIR}
    . common/cleanup.sh
}

############################ print_cu_data #############################
# local shell function to print certutil input data
########################################################################
print_cu_data()
{
    echo "=== Certutil input data ==="
    cat ${CU_DATA}
    echo "==="
}

set_cert_sn()
{
    if [ -z "${SERIAL}" ]; then
        CERT_SN_CNT=$(expr ${CERT_SN_CNT} + 1)
        CERT_SN=${CERT_SN_CNT}
    else
        echo ${SERIAL} | cut -b 1 | grep '+' > /dev/null
        if [ $? -eq 0 ]; then
            CERT_SN=$(echo ${SERIAL} | cut -b 2-)
            CERT_SN=$(expr ${CERT_SN_FIX} + ${CERT_SN})
        else
            CERT_SN=${SERIAL}
        fi 
    fi
}

############################# create_db ################################
# local shell function to create certificate database
########################################################################
create_db()
{
    DB=$1

    [ -d "${DB}" ] && rm -rf ${DB}
    mkdir -p ${DB}

    echo "${DB}passwd" > ${DB}/dbpasswd

    TESTNAME="Creating DB ${DB}"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "certutil -N -d ${DB} -f ${DB}/dbpasswd" 
    ${BINDIR}/certutil -N -d ${DB} -f ${DB}/dbpasswd
    html_msg $? 0 "${SCENARIO}${TESTNAME}" 
}

########################### create_root_ca #############################
# local shell function to generate self-signed root certificate
########################################################################
create_root_ca()
{
    ENTITY=$1
    ENTITY_DB=${ENTITY}DB

    set_cert_sn
    date >> ${NOISE_FILE} 2>&1

    CTYPE_OPT=
    if [ -n "${CTYPE}" ]; then
        CTYPE_OPT="-k ${CTYPE}"
    fi

    echo "5
6
9
n
y
-1
n
5
6
7
9
n
" > ${CU_DATA}

    TESTNAME="Creating Root CA ${ENTITY}"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "certutil -s \"CN=${ENTITY} ROOT CA, O=${ENTITY}, C=US\" -S -n ${ENTITY} ${CTYPE_OPT} -t CTu,CTu,CTu -v 600 -x -d ${ENTITY_DB} -1 -2 -5 -f ${ENTITY_DB}/dbpasswd -z ${NOISE_FILE} -m ${CERT_SN} < ${CU_DATA}"
    print_cu_data
    ${BINDIR}/certutil -s "CN=${ENTITY} ROOT CA, O=${ENTITY}, C=US" -S -n ${ENTITY} ${CTYPE_OPT} -t CTu,CTu,CTu -v 600 -x -d ${ENTITY_DB} -1 -2 -5 -f ${ENTITY_DB}/dbpasswd -z ${NOISE_FILE} -m ${CERT_SN} < ${CU_DATA}
    html_msg $? 0 "${SCENARIO}${TESTNAME}"

    TESTNAME="Exporting Root CA ${ENTITY}.der"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "certutil -L -d ${ENTITY_DB} -r -n ${ENTITY} -o ${ENTITY}.der"
    ${BINDIR}/certutil -L -d ${ENTITY_DB} -r -n ${ENTITY} -o ${ENTITY}.der
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}

########################### create_cert_req ############################
# local shell function to generate certificate sign request
########################################################################
create_cert_req()
{
    ENTITY=$1
    TYPE=$2

    ENTITY_DB=${ENTITY}DB

    REQ=${ENTITY}Req.der

    date >> ${NOISE_FILE} 2>&1

    CTYPE_OPT=
    if [ -n "${CTYPE}" ]; then
        CTYPE_OPT="-k ${CTYPE}"
    fi

    CA_FLAG=
    EXT_DATA=
    OPTIONS=

    if [ "${TYPE}" != "EE" ]; then
        CA_FLAG="-2"
        EXT_DATA="y
-1
y
"
    fi

    process_crldp

    echo "${EXT_DATA}" > ${CU_DATA}

    TESTNAME="Creating ${TYPE} certifiate request ${REQ}"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "certutil -s \"CN=${ENTITY} ${TYPE}, O=${ENTITY}, C=US\" ${CTYPE_OPT} -R ${CA_FLAG} -d ${ENTITY_DB} -f ${ENTITY_DB}/dbpasswd -z ${NOISE_FILE} -o ${REQ} ${OPTIONS} < ${CU_DATA}"
    print_cu_data
    ${BINDIR}/certutil -s "CN=${ENTITY} ${TYPE}, O=${ENTITY}, C=US" ${CTYPE_OPT} -R ${CA_FLAG} -d ${ENTITY_DB} -f ${ENTITY_DB}/dbpasswd -z ${NOISE_FILE} -o ${REQ} ${OPTIONS} < ${CU_DATA} 
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}

############################ create_entity #############################
# local shell function to create certificate chain entity
########################################################################
create_entity()
{
    ENTITY=$1
    TYPE=$2

    if [ -z "${ENTITY}" ]; then
        echo "Configuration error: Unnamed entity"
        exit 1
    fi

    DB=${ENTITY}DB
    ENTITY_DB=${ENTITY}DB

    case "${TYPE}" in
    "Root")
        create_db "${DB}"
        create_root_ca "${ENTITY}"
        ;;
    "Intermediate" | "Bridge" | "EE")
        create_db "${DB}"
        create_cert_req "${ENTITY}" "${TYPE}"
        ;;
    "*")
        echo "Configuration error: Unknown type ${TYPE}"
        exit 1
        ;;
    esac
}

########################################################################
# List of global variables related to certificate extensions processing:
#
# Generated by process_extensions and functions called from it:
# OPTIONS - list of command line policy extensions 
# DATA - list of inpud data related to policy extensions
#
# Generated by parse_config:
# POLICY - list of certificate policies
# MAPPING - list of policy mappings 
# INHIBIT - inhibit flag
# AIA - AIA list
########################################################################

############################ process_policy ############################
# local shell function to process policy extension parameters and 
# generate input for certutil
########################################################################
process_policy()
{
    if [ -n "${POLICY}" ]; then
        OPTIONS="${OPTIONS} --extCP"

        NEXT=
        for ITEM in ${POLICY}; do
            if [ -n "${NEXT}" ]; then
                DATA="${DATA}y
"
            fi

            NEXT=1
            DATA="${DATA}${ITEM}
1

n
"
        done

        DATA="${DATA}n
n
"
    fi
}

########################### process_mapping ############################
# local shell function to process policy mapping parameters and 
# generate input for certutil
########################################################################
process_mapping()
{
    if [ -n "${MAPPING}" ]; then
        OPTIONS="${OPTIONS} --extPM"

        NEXT=
        for ITEM in ${MAPPING}; do
            if [ -n "${NEXT}" ]; then
                DATA="${DATA}y
"
            fi

            NEXT=1
            IDP=`echo ${ITEM} | cut -d: -f1`
            SDP=`echo ${ITEM} | cut -d: -f2`
            DATA="${DATA}${IDP}
${SDP}
"
        done

        DATA="${DATA}n
n
"
    fi
}

########################### process_inhibit#############################
# local shell function to process inhibit extension and generate input 
# for certutil
########################################################################
process_inhibit()
{
    if [ -n "${INHIBIT}" ]; then
        OPTIONS="${OPTIONS} --extIA"

        DATA="${DATA}${INHIBIT}
n
"
    fi
}

############################# process_aia ##############################
# local shell function to process AIA extension parameters and 
# generate input for certutil
########################################################################
process_aia()
{
    if [ -n "${AIA}" ]; then
        OPTIONS="${OPTIONS} --extAIA"

        DATA="${DATA}1
"

        for ITEM in ${AIA}; do
            PK7_NONCE=`expr $PK7_NONCE + 1`

            echo ${ITEM} | grep ":" > /dev/null
            if [ $? -eq 0 ]; then
                CERT_NICK=`echo ${ITEM} | cut -d: -f1`
                CERT_ISSUER=`echo ${ITEM} | cut -d: -f2`
                CERT_LOCAL="${CERT_NICK}${CERT_ISSUER}.der"
                CERT_PUBLIC="${HOST}-$$-${CERT_NICK}${CERT_ISSUER}-${PK7_NONCE}.der"
            else
                CERT_LOCAL="${ITEM}.p7"
                CERT_PUBLIC="${HOST}-$$-${ITEM}-${PK7_NONCE}.p7"
            fi

            DATA="${DATA}7
${NSS_AIA_HTTP}/${CERT_PUBLIC}
"

            if [ -n "${NSS_AIA_PATH}" ]; then
                cp ${CERT_LOCAL} ${NSS_AIA_PATH}/${CERT_PUBLIC} 2> /dev/null
                chmod a+r ${NSS_AIA_PATH}/${CERT_PUBLIC}
                echo ${NSS_AIA_PATH}/${CERT_PUBLIC} >> ${AIA_FILES}
            fi
        done

        DATA="${DATA}0
n
n"
    fi
}

process_ocsp()
{
    if [ -n "${OCSP}" ]; then
        OPTIONS="${OPTIONS} --extAIA"

        DATA="${DATA}2
7
${NSS_AIA_OCSP}:${OCSP}
0
n
n
"
    fi
}

process_crldp()
{
    if [ -n "${CRLDP}" ]; then
        OPTIONS="${OPTIONS} -4"

        EXT_DATA="${EXT_DATA}1
"

        for ITEM in ${CRLDP}; do
            CRL_PUBLIC="${HOST}-$$-${ITEM}-${SCEN_CNT}.crl"

            EXT_DATA="${EXT_DATA}7
${NSS_AIA_HTTP}/${CRL_PUBLIC}
"
        done

        EXT_DATA="${EXT_DATA}-1
-1
-1
n
n
"
    fi
}

process_ku_ns_eku()
{
    if [ -n "${EXT_KU}" ]; then
        OPTIONS="${OPTIONS} --keyUsage ${EXT_KU}"
    fi
    if [ -n "${EXT_NS}" ]; then
        EXT_NS_KEY=$(echo ${EXT_NS} | cut -d: -f1)
        EXT_NS_CODE=$(echo ${EXT_NS} | cut -d: -f2)

        OPTIONS="${OPTIONS} --nsCertType ${EXT_NS_KEY}"
        DATA="${DATA}${EXT_NS_CODE}
-1
n
"
    fi
    if [ -n "${EXT_EKU}" ]; then
        OPTIONS="${OPTIONS} --extKeyUsage ${EXT_EKU}"
    fi
}

copy_crl()

{
    if [ -z "${NSS_AIA_PATH}" ]; then
        return;
    fi

    CRL_LOCAL="${COPYCRL}.crl"
    CRL_PUBLIC="${HOST}-$$-${COPYCRL}-${SCEN_CNT}.crl"

    cp ${CRL_LOCAL} ${NSS_AIA_PATH}/${CRL_PUBLIC} 2> /dev/null
    chmod a+r ${NSS_AIA_PATH}/${CRL_PUBLIC}
    echo ${NSS_AIA_PATH}/${CRL_PUBLIC} >> ${AIA_FILES}
}

########################## process_extension ###########################
# local shell function to process entity extension parameters and 
# generate input for certutil
########################################################################
process_extensions()
{
    OPTIONS=
    DATA=

    process_policy
    process_mapping
    process_inhibit
    process_aia
    process_ocsp
    process_ku_ns_eku
}

############################## sign_cert ###############################
# local shell function to sign certificate sign reuqest
########################################################################
sign_cert()
{
    ENTITY=$1
    ISSUER=$2
    TYPE=$3

    [ -z "${ISSUER}" ] && return

    ENTITY_DB=${ENTITY}DB
    ISSUER_DB=${ISSUER}DB
    REQ=${ENTITY}Req.der
    CERT=${ENTITY}${ISSUER}.der

    set_cert_sn

    EMAIL_OPT=
    if [ "${TYPE}" = "Bridge" ]; then
        EMAIL_OPT="-7 ${ENTITY}@${ISSUER}"

        [ -n "${EMAILS}" ] && EMAILS="${EMAILS},"
        EMAILS="${EMAILS}${ENTITY}@${ISSUER}"
    fi

    process_extensions 

    echo "${DATA}" > ${CU_DATA}

    TESTNAME="Creating certficate ${CERT} signed by ${ISSUER}"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "certutil -C -c ${ISSUER} -v 60 -d ${ISSUER_DB} -i ${REQ} -o ${CERT} -f ${ISSUER_DB}/dbpasswd -m ${CERT_SN} ${EMAIL_OPT} ${OPTIONS} < ${CU_DATA}"
    print_cu_data
    ${BINDIR}/certutil -C -c ${ISSUER} -v 60 -d ${ISSUER_DB} -i ${REQ} -o ${CERT} -f ${ISSUER_DB}/dbpasswd -m ${CERT_SN} ${EMAIL_OPT} ${OPTIONS} < ${CU_DATA}
    html_msg $? 0 "${SCENARIO}${TESTNAME}"

    TESTNAME="Importing certificate ${CERT} to ${ENTITY_DB} database"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "certutil -A -n ${ENTITY} -t u,u,u -d ${ENTITY_DB} -f ${ENTITY_DB}/dbpasswd -i ${CERT}"
    ${BINDIR}/certutil -A -n ${ENTITY} -t u,u,u -d ${ENTITY_DB} -f ${ENTITY_DB}/dbpasswd -i ${CERT}
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}

############################# create_pkcs7##############################
# local shell function to package bridge certificates into pkcs7 
# package
########################################################################
create_pkcs7()
{
    ENTITY=$1
    ENTITY_DB=${ENTITY}DB

    TESTNAME="Generating PKCS7 package from ${ENTITY_DB} database"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "cmsutil -O -r \"${EMAILS}\" -d ${ENTITY_DB} > ${ENTITY}.p7"
    ${BINDIR}/cmsutil -O -r "${EMAILS}" -d ${ENTITY_DB} > ${ENTITY}.p7
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}

############################# import_key ###############################
# local shell function to import private key + cert into database
########################################################################
import_key()
{
    KEY_NAME=$1.p12
    DB=$2

    KEY_FILE=${QADIR}/libpkix/certs/${KEY_NAME}

    TESTNAME="Importing p12 key ${KEY_NAME} to ${DB} database"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "${BINDIR}/pk12util -d ${DB} -i ${KEY_FILE} -k ${DB}/dbpasswd -W nssnss"
    ${BINDIR}/pk12util -d ${DB} -i ${KEY_FILE} -k ${DB}/dbpasswd -W nssnss
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}


############################# import_cert ##############################
# local shell function to import certificate into database
########################################################################
import_cert()
{
    IMPORT=$1
    DB=$2

    CERT_NICK=`echo ${IMPORT} | cut -d: -f1`
    CERT_ISSUER=`echo ${IMPORT} | cut -d: -f2`
    CERT_TRUST=`echo ${IMPORT} | cut -d: -f3`

    if [ "${CERT_ISSUER}" = "x" ]; then
        CERT_ISSUER=
        CERT=${CERT_NICK}.cert
        CERT_FILE="${QADIR}/libpkix/certs/${CERT}"
    else
        CERT=${CERT_NICK}${CERT_ISSUER}.der
        CERT_FILE=${CERT}
    fi

    IS_ASCII=`grep -c -- "-----BEGIN CERTIFICATE-----" ${CERT_FILE}`

    ASCII_OPT=
    if [ "${IS_ASCII}" -gt 0 ]; then
        ASCII_OPT="-a"
    fi
   
    TESTNAME="Importing certificate ${CERT} to ${DB} database"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "certutil -A -n ${CERT_NICK} ${ASCII_OPT} -t \"${CERT_TRUST}\" -d ${DB} -f ${DB}/dbpasswd -i ${CERT_FILE}"
    ${BINDIR}/certutil -A -n ${CERT_NICK} ${ASCII_OPT} -t "${CERT_TRUST}" -d ${DB} -f ${DB}/dbpasswd -i ${CERT_FILE} 
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}

import_crl()
{
    IMPORT=$1
    DB=$2

    CRL_NICK=`echo ${IMPORT} | cut -d: -f1`
    CRL_FILE=${CRL_NICK}.crl

    if [ ! -f "${CRL_FILE}" ]; then
        return
    fi 

    TESTNAME="Importing CRL ${CRL_FILE} to ${DB} database"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "crlutil -I -d ${DB} -f ${DB}/dbpasswd -i ${CRL_FILE}"
    ${BINDIR}/crlutil -I -d ${DB} -f ${DB}/dbpasswd -i ${CRL_FILE} 
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}

create_crl()
{
    ISSUER=$1
    ISSUER_DB=${ISSUER}DB

    CRL=${ISSUER}.crl

    DATE=$(date -u '+%Y%m%d%H%M%SZ')
    UPDATE=$(expr $(date -u '+%Y') + 1)$(date -u '+%m%d%H%M%SZ')

    echo "update=${DATE}" > ${CRL_DATA}
    echo "nextupdate=${UPDATE}" >> ${CRL_DATA}

    TESTNAME="Create CRL for ${ISSUER_DB}"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "crlutil -G -d ${ISSUER_DB} -n ${ISSUER} -f ${ISSUER_DB}/dbpasswd -o ${CRL}"
    echo "=== Crlutil input data ==="
    cat ${CRL_DATA}
    echo "==="
    ${BINDIR}/crlutil -G -d ${ISSUER_DB} -n ${ISSUER} -f ${ISSUER_DB}/dbpasswd -o ${CRL} < ${CRL_DATA}
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}

revoke_cert()
{
    ISSUER=$1
    ISSUER_DB=${ISSUER}DB

    CRL=${ISSUER}.crl

    set_cert_sn

    sleep 1
    DATE=$(date -u '+%Y%m%d%H%M%SZ')
    echo "update=${DATE}" > ${CRL_DATA}
    echo "addcert ${CERT_SN} ${DATE}" >> ${CRL_DATA}

    TESTNAME="Revoking certificate with SN ${CERT_SN} issued by ${ISSUER}"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "crlutil -M -d ${ISSUER_DB} -n ${ISSUER} -f ${ISSUER_DB}/dbpasswd -o ${CRL}"
    echo "=== Crlutil input data ==="
    cat ${CRL_DATA}
    echo "==="
    ${BINDIR}/crlutil -M -d ${ISSUER_DB} -n ${ISSUER} -f ${ISSUER_DB}/dbpasswd -o ${CRL} < ${CRL_DATA}
    html_msg $? 0 "${SCENARIO}${TESTNAME}"
}

########################################################################
# List of global variables related to certificate verification:
#
# Generated by parse_config:
# DB - DB used for testing
# FETCH - fetch flag (used with AIA extension)
# POLICY - list of policies
# TRUST - trust anchor
# VERIFY - list of certificates to use as vfychain parameters
# EXP_RESULT - expected result
# REV_OPTS - revocation options
########################################################################

############################# verify_cert ##############################
# local shell function to verify certificate validity
########################################################################
verify_cert()
{
    DB_OPT=
    FETCH_OPT=
    POLICY_OPT=
    TRUST_OPT=
    VFY_CERTS=
    VFY_LIST=

    if [ -n "${DB}" ]; then
        DB_OPT="-d ${DB}"
    fi

    if [ -n "${FETCH}" ]; then
        FETCH_OPT="-f"
        if [ -z "${NSS_AIA_HTTP}" ]; then
            echo "${SCRIPTNAME} Skipping test using AIA fetching, NSS_AIA_HTTP not defined"
            return
        fi
    fi

    for ITEM in ${POLICY}; do
        POLICY_OPT="${POLICY_OPT} -o ${ITEM}"
    done

    for ITEM in ${TRUST}; do
        echo ${ITEM} | grep ":" > /dev/null
        if [ $? -eq 0 ]; then
            CERT_NICK=`echo ${ITEM} | cut -d: -f1`
            CERT_ISSUER=`echo ${ITEM} | cut -d: -f2`
            CERT=${CERT_NICK}${CERT_ISSUER}.der

            TRUST_OPT="${TRUST_OPT} -t ${CERT}"
        else
            TRUST_OPT="${TRUST_OPT} -t ${ITEM}"
        fi
    done

    for ITEM in ${VERIFY}; do
        CERT_NICK=`echo ${ITEM} | cut -d: -f1`
        CERT_ISSUER=`echo ${ITEM} | cut -d: -f2`

        if [ "${CERT_ISSUER}" = "x" ]; then
            CERT="${QADIR}/libpkix/certs/${CERT_NICK}.cert"
            VFY_CERTS="${VFY_CERTS} ${CERT}"
            VFY_LIST="${VFY_LIST} ${CERT_NICK}.cert"
        else
            CERT=${CERT_NICK}${CERT_ISSUER}.der
            VFY_CERTS="${VFY_CERTS} ${CERT}"
            VFY_LIST="${VFY_LIST} ${CERT}"
        fi
    done

    VFY_OPTS_TNAME="${REV_OPTS} ${DB_OPT} ${FETCH_OPT} ${USAGE_OPT} ${POLICY_OPT} ${TRUST_OPT}"
    VFY_OPTS_ALL="${DB_OPT} -pp -vv ${REV_OPTS} ${FETCH_OPT} ${USAGE_OPT} ${POLICY_OPT} ${VFY_CERTS} ${TRUST_OPT}"

    TESTNAME="Verifying certificate(s) ${VFY_LIST} with flags ${VFY_OPTS_TNAME}"
    echo "${SCRIPTNAME}: ${TESTNAME}"
    echo "vfychain ${VFY_OPTS_ALL}"

    if [ -z "${MEMLEAK_DBG}" ]; then
        VFY_OUT=$(${BINDIR}/vfychain ${VFY_OPTS_ALL} 2>&1)
        RESULT=$?
        echo "${VFY_OUT}"
    else 
        VFY_OUT=$(${RUN_COMMAND_DBG} ${BINDIR}/vfychain ${VFY_OPTS_ALL} 2>> ${LOGFILE})
        RESULT=$?
        echo "${VFY_OUT}"
    fi

    echo "${VFY_OUT}" | grep "ERROR -5990: I/O operation timed out" > /dev/null
    E5990=$?
    echo "${VFY_OUT}" | grep "ERROR -8030: Server returned bad HTTP response" > /dev/null
    E8030=$?

    if [ $E5990 -eq 0 -o $E8030 -eq 0 ]; then
        echo "Result of this test is not valid due to network time out."
        html_unknown "${SCENARIO}${TESTNAME}"
        return
    fi

    echo "Returned value is ${RESULT}, expected result is ${EXP_RESULT}"
    
    if [ "${EXP_RESULT}" = "pass" -a ${RESULT} -eq 0 ]; then
        html_passed "${SCENARIO}${TESTNAME}"
    elif [ "${EXP_RESULT}" = "fail" -a ${RESULT} -ne 0 ]; then
        html_passed "${SCENARIO}${TESTNAME}"
    else
        html_failed "${SCENARIO}${TESTNAME}"
    fi
}

check_ocsp()
{
    OCSP_CERT=$1

    CERT_NICK=`echo ${OCSP_CERT} | cut -d: -f1`
    CERT_ISSUER=`echo ${OCSP_CERT} | cut -d: -f2`

    if [ "${CERT_ISSUER}" = "x" ]; then
        CERT_ISSUER=
        CERT=${CERT_NICK}.cert
        CERT_FILE="${QADIR}/libpkix/certs/${CERT}"
    else
        CERT=${CERT_NICK}${CERT_ISSUER}.der
        CERT_FILE=${CERT}
    fi

    OCSP_HOST=$(${BINDIR}/pp -t certificate -i ${CERT_FILE} | grep URI | sed "s/.*:\/\///" | sed "s/:.*//")

    if [ "${OS_ARCH}" = "WINNT" ]; then
        ping -n 1 ${OCSP_HOST}
        return $?
    elif [ "${OS_ARCH}" = "HP-UX" ]; then
        ping ${OCSP_HOST} -n 1
        return $?
    else
        ping -c 1 ${OCSP_HOST}
        return $?
    fi
}

############################ parse_result ##############################
# local shell function to process expected result value
# this function was created for case that expected result depends on
# some conditions - in our case type of cert DB
#
# default results are pass and fail
# this function added parsable values in format:
# type1:value1 type2:value2 .... typex:valuex
#
# allowed types are dbm, sql, all (all means all other cases)
# allowed values are pass and fail
#
# if this format is not used, EXP_RESULT will stay unchanged (this also
# covers pass and fail states)
########################################################################
parse_result()
{
    for RES in ${EXP_RESULT}
    do
        RESTYPE=$(echo ${RES} | cut -d: -f1)
        RESSTAT=$(echo ${RES} | cut -d: -f2)

        if [ "${RESTYPE}" = "${NSS_DEFAULT_DB_TYPE}" -o "${RESTYPE}" = "all" ]; then
            EXP_RESULT=${RESSTAT}
            break
        fi
    done
}

############################ parse_config ##############################
# local shell function to parse and process file containing certificate
# chain configuration and list of tests
########################################################################
parse_config()
{
    SCENARIO=
    LOGNAME=

    while read KEY VALUE
    do
        case "${KEY}" in
        "entity")
            ENTITY="${VALUE}"
            TYPE=
            ISSUER=
            CTYPE=
            POLICY=
            MAPPING=
            INHIBIT=
            AIA=
            CRLDP=
            OCSP=
            DB=
            EMAILS=
            EXT_KU=
            EXT_NS=
            EXT_EKU=
            SERIAL=
            ;;
        "type")
            TYPE="${VALUE}"
            ;;
        "issuer")
            if [ -n "${ISSUER}" ]; then
                if [ -z "${DB}" ]; then
                    create_entity "${ENTITY}" "${TYPE}"
                fi
                sign_cert "${ENTITY}" "${ISSUER}" "${TYPE}"
            fi

            ISSUER="${VALUE}"
            POLICY=
            MAPPING=
            INHIBIT=
            AIA=
            EXT_KU=
            EXT_NS=
            EXT_EKU=
            ;;
        "ctype") 
            CTYPE="${VALUE}"
            ;;
        "policy")
            POLICY="${POLICY} ${VALUE}"
            ;;
        "mapping")
            MAPPING="${MAPPING} ${VALUE}"
            ;;
        "inhibit")
            INHIBIT="${VALUE}"
            ;;
        "aia")
            AIA="${AIA} ${VALUE}"
            ;;
        "crldp")
            CRLDP="${CRLDP} ${VALUE}"
            ;;
        "ocsp")
            OCSP="${VALUE}"
            ;;
        "db")
            DB="${VALUE}DB"
            create_db "${DB}"
            ;;
        "import")
            IMPORT="${VALUE}"
            import_cert "${IMPORT}" "${DB}"
            import_crl "${IMPORT}" "${DB}"
            ;;
        "import_key")
            IMPORT="${VALUE}"
            import_key "${IMPORT}" "${DB}"
            ;;
        "crl")
            ISSUER="${VALUE}"
            create_crl "${ISSUER}"
            ;;
        "revoke")
            REVOKE="${VALUE}"
            ;;
        "serial")
            SERIAL="${VALUE}"
            ;;
        "copycrl")
            COPYCRL="${VALUE}"
            copy_crl "${COPYCRL}"
            ;;
        "verify")
            VERIFY="${VALUE}"
            TRUST=
            POLICY=
            FETCH=
            EXP_RESULT=
            REV_OPTS=
            USAGE_OPT=
            ;;
        "cert")
            VERIFY="${VERIFY} ${VALUE}"
            ;;
        "testdb")
            if [ -n "${VALUE}" ]; then
                DB="${VALUE}DB"
            else
                DB=
            fi
            ;;
        "trust")
            TRUST="${TRUST} ${VALUE}"
            ;;
        "fetch")
            FETCH=1
            ;;
        "result")
            EXP_RESULT="${VALUE}"
            parse_result
            ;;
        "rev_type")
            REV_OPTS="${REV_OPTS} -g ${VALUE}"
            ;;
        "rev_flags")
            REV_OPTS="${REV_OPTS} -h ${VALUE}"
            ;;
        "rev_mtype")
            REV_OPTS="${REV_OPTS} -m ${VALUE}"
            ;;
        "rev_mflags")
            REV_OPTS="${REV_OPTS} -s ${VALUE}"
            ;;
        "scenario")
            SCENARIO="${VALUE}: "

            CHAINS_DIR="${HOSTDIR}/chains/${VALUE}"
            mkdir -p ${CHAINS_DIR}
            cd ${CHAINS_DIR}

            if [ -n "${MEMLEAK_DBG}" ]; then
                LOGNAME="libpkix-${VALUE}"
                LOGFILE="${LOGDIR}/${LOGNAME}"
            fi

            SCEN_CNT=$(expr ${SCEN_CNT} + 1)
            ;;
        "sleep")
            sleep ${VALUE}
            ;;
        "break")
            break
            ;;
        "check_ocsp")
            check_ocsp ${VALUE}
            if [ $? -ne 0 ]; then
                echo "OCSP server not accessible, skipping OCSP tests"
                break;
            fi
            ;;
        "ku")
            EXT_KU="${VALUE}"
            ;;
        "ns")
            EXT_NS="${VALUE}"
            ;;
        "eku")
            EXT_EKU="${VALUE}"
            ;;
        "usage")
            USAGE_OPT="-u ${VALUE}"
            ;;
        "")
            if [ -n "${ENTITY}" ]; then
                if [ -z "${DB}" ]; then
                    create_entity "${ENTITY}" "${TYPE}"
                fi
                sign_cert "${ENTITY}" "${ISSUER}" "${TYPE}"
                if [ "${TYPE}" = "Bridge" ]; then
                    create_pkcs7 "${ENTITY}"
                fi
                ENTITY=
            fi

            if [ -n "${VERIFY}" ]; then
                verify_cert
                VERIFY=
            fi

            if [ -n "${REVOKE}" ]; then
                revoke_cert "${REVOKE}" "${DB}"
                REVOKE=
            fi
            ;;
        *)
            if [ `echo ${KEY} | cut -b 1` != "#" ]; then
                echo "Configuration error: Unknown keyword ${KEY}"
                exit 1
            fi
            ;;
        esac
    done

    if [ -n "${MEMLEAK_DBG}" ]; then
        log_parse
        html_msg $? 0 "${SCENARIO}Memory leak checking" 
    fi
}

############################# chains_main ##############################
# local shell function to process all testing scenarios
########################################################################
chains_main()
{
    while read LINE 
    do
        > ${AIA_FILES}

        parse_config < "${QADIR}/chains/scenarios/${LINE}"

        while read AIA_FILE
        do
            rm ${AIA_FILE} 2> /dev/null
        done < ${AIA_FILES}
        rm ${AIA_FILES}
    done < "${CHAINS_SCENARIOS}"
}

################################ main ##################################

chains_init
chains_main
chains_cleanup

