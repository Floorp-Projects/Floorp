#! /bin/sh

########################################################################
#
# mozilla/security/nss/tests/cert/cert.sh
#
# Certificate generating and handeling for NSS QA, can be included 
# multiple times from all.sh and the individual scripts
#
# needs to work on all Unix and Windows platforms
#
# included from (don't expect this to be up to date)
# --------------------------------------------------
#   all.sh
#   ssl.sh
#   smime.sh
#   tools.sh
#
# special strings
# ---------------
#   FIXME ... known problems, search for this string
#   NOTE .... unexpected behavior
#
# FIXME - Netscape - NSS
########################################################################

if [ -z "${INIT_SOURCED}" ] ; then
    . ../common/init.sh
fi

TEMPFILES="${PWFILE} ${CERTSCRIPT} ${NOISE_FILE}"

trap "rm -f ${TEMPFILES}; Exit $0 Signal_caught" 2 3

certlog() ######################    write the cert_status file
{
    echo $* >>${CERT_LOG_FILE}
}

html() #########################    write the results.html file
{
    echo $* >>${RESULTS}
}

################################ noise ##################################
# Generate noise for our certs
#
# NOTE: these keys are only suitable for testing, as this whole thing bypasses
# the entropy gathering. Don't use this method to generate keys and certs for
# product use or deployment.
#########################################################################
noise()
{
    netstat >> ${NOISE_FILE} 2>&1
    date >> ${NOISE_FILE} 2>&1
}

################################ certu #################################
# local shell function to call certutil, also: writes action and options to
# stdout, sets variable RET and writes results to the html file results
########################################################################
certu()
{
    echo "$SCRIPTNAME: ${CU_ACTION}"

    if [ -n "${CU_SUBJECT}" ]; then
        #the subject of the cert contains blanks, and the shell 
        #will strip the quotes off the string, if called otherwise...
        echo "certutil -s \"${CU_SUBJECT}\" $*"
        certutil -s "${CU_SUBJECT}" $*
    	CU_SUBJECT=""
    else
        echo "certutil $*"
        certutil $*
    fi
    RET=$?
    if [ "$RET" -ne 0 ]; then
	CERTFAILED=$RET
        html "<TR><TD>${CU_ACTION} ($RET) $CU_FAILED"
        certlog "ERROR: ${CU_ACTION} failed $RET"
    else
        html "<TR><TD>${CU_ACTION}$CU_PASSED"
    fi
    return $RET
}

################################ init_cert #############################
# local shell function to initialize creation of client and server certs
########################################################################
init_cert()
{
    CERTDIR="$1"
    CERTNAME="$2"
    CERTSERIAL="$3"

    if [ ! -d "${CERTDIR}" ]; then
        mkdir -p "${CERTDIR}"
    else
        echo "WARNING - ${CERTDIR} exists"
    fi
    cd "${CERTDIR}"

    noise
}

################################ create_cert ###########################
# local shell function to create client certs 
#     initialize DB, import
#     root cert
#     generate request
#     sign request
#     import Cert
#
########################################################################
create_cert()
{
    init_cert "$1" "$2" "$3"

    CU_ACTION="Initializing ${CERTNAME}'s Cert DB"
    certu -N -d "${CERTDIR}" -f "${PWFILE}" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Import Root CA for $CERTNAME"
    certu -A -n "TestCA" -t "TC,TC,TC" -f "${PWFILE}" -d "${CERTDIR}" -i "${CADIR}/root.cert" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Generate Cert Request for $CERTNAME"
    CU_SUBJECT="CN=$CERTNAME, E=${CERTNAME}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    certu -R -d "${CERTDIR}" -f "${PWFILE}" -z "${NOISE_FILE}" -o req 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Sign ${CERTNAME}'s Request"
    certu -C -c "TestCA" -m "$CERTSERIAL" -v 60 -d "${CADIR}" -i req -o "${CERTNAME}.cert" -f "${PWFILE}" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Import $CERTNAME's Cert"
    certu -A -n "$CERTNAME" -t "u,u,u" -d "${CERTDIR}" -f "${PWFILE}" -i "${CERTNAME}.cert" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    certlog "SUCCESS: $CERTNAME's Cert Created"
    return 0
}

################## main #################################################

certlog "started"
html "<TABLE BORDER=1><TR><TH COLSPAN=3>Certutil Tests</TH></TR>"
html "<TR><TH width=500>Test Case</TH><TH width=50>Result</TH></TR>"

################## Generate noise for our CA cert. ######################
# NOTE: these keys are only suitable for testing, as this whole thing bypasses
# the entropy gathering. Don't use this method to generate keys and certs for
# product use or deployment.
#
ps -efl > ${NOISE_FILE} 2>&1
ps aux >> ${NOISE_FILE} 2>&1
noise

################# Temp. Certificate Authority (CA) #######################
#
# build the TEMP CA used for testing purposes
#
################# Creating a CA Certificate ##############################
#
echo "********************** Creating a CA Certificate **********************"

if [ ! -d "${CADIR}" ]; then
	mkdir -p "${CADIR}"
fi
cd ${CADIR}

echo nss > ${PWFILE}

CU_ACTION="Creating CA Cert DB"
certu -N -d ${CADIR} -f ${PWFILE} 2>&1
if [ "$RET" -ne 0 ]; then
    exit 3 #with errorcode
fi

################# Generating Certscript #################################
#
echo "$SCRIPTNAME: Certificate initialized, generating script"

echo 5 > ${CERTSCRIPT}
echo 9 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}
echo y >> ${CERTSCRIPT}
echo 3 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}
echo 5 >> ${CERTSCRIPT}
echo 6 >> ${CERTSCRIPT}
echo 7 >> ${CERTSCRIPT}
echo 9 >> ${CERTSCRIPT}
echo n >> ${CERTSCRIPT}

################# Creating CA Cert ######################################
#
CU_ACTION="Creating CA Cert"
CU_SUBJECT="CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
certu -S -n "TestCA" -t "CTu,CTu,CTu" -v 60 -x -d ${CADIR} -1 -2 -5 -f ${PWFILE} -z ${NOISE_FILE} < ${CERTSCRIPT} 2>&1
if [ "$RET" -ne 0 ]; then
    exit 1 #with errorcode
fi

################# Exporting Root Cert ###################################
#
CU_ACTION="Exporting Root Cert"
certu -L -n "TestCA" -r -d ${CADIR} -o ${CADIR}/root.cert 
if [ "$RET" -ne 0 ]; then
    exit 2 #with errorcode
fi

################# Creating Certificates for S/MIME tests ################
#
CERTFAILED=0
echo "**************** Creating Client CA Issued Certificates ****************"

create_cert ${ALICEDIR} "Alice" 3
create_cert ${BOBDIR} "Bob" 4

echo "**************** Creating Dave's Certificate ****************"
init_cert "${DAVEDIR}" Dave 5
cp ${CADIR}/*.db .

#########################################################################
#
CU_ACTION="Creating ${CERTNAME}'s Server Cert"
CU_SUBJECT="CN=${CERTNAME}, E=${CERTNAME}@bogus.com, O=BOGUS Netscape, L=Mountain View, ST=California, C=US"
certu -S -n "${CERTNAME}" -c "TestCA" -t "u,u,u" -m "$CERTSERIAL" -d "${CERTDIR}" -f "${PWFILE}" -z "${NOISE_FILE}" -v 60 2>&1

CU_ACTION="Export Dave's Cert"
certu -L -n "Dave" -r -d ${DAVEDIR} -o Dave.cert

################# Importing Certificates for S/MIME tests ###############
#
echo "**************** Importing Certificates *********************"
CU_ACTION="Import Alices's cert into Bob's db"
certu -E -t "u,u,u" -d ${BOBDIR} -f ${PWFILE} -i ${ALICEDIR}/Alice.cert 2>&1

CU_ACTION="Import Bob's cert into Alice's db"
certu -E -t "u,u,u" -d ${ALICEDIR} -f ${PWFILE} -i ${BOBDIR}/Bob.cert 2>&1

CU_ACTION="Import Dave's cert into Alice's DB"
certu -E -t "u,u,u" -d ${ALICEDIR} -f ${PWFILE} -i ${DAVEDIR}/Dave.cert 2>&1

CU_ACTION="Import Dave's cert into Bob's DB"
certu -E -t "u,u,u" -d ${BOBDIR} -f ${PWFILE} -i ${DAVEDIR}/Dave.cert 2>&1

if [ "$CERTFAILED" != 0 ] ; then
    certlog "ERROR: SMIME failed $RET"
else
    certlog "SUCCESS: SMIME passed"
fi

################# Creating Certs for SSL test ###########################
#
CERTFAILED=0
echo "**************** Creating Client CA Issued Certificates ****************"
create_cert ${CLIENTDIR} "TestUser" 6

echo "***** Creating Server CA Issued Certificate for ${HOST}.${DOMSUF} *****"
init_cert ${SERVERDIR} "${HOST}.${DOMSUF}" 1
cp ${CADIR}/*.db .
CU_ACTION="Creating ${CERTNAME}'s Server Cert"
CU_SUBJECT="CN=${CERTNAME}, O=BOGUS Netscape, L=Mountain View, ST=California, C=US"
certu -S -n "${CERTNAME}" -c "TestCA" -t "Pu,Pu,Pu" -d "${CERTDIR}" -f "${PWFILE}" -z "${NOISE_FILE}" -v 60 2>&1
#certu -S -n "${CERTNAME}" -c "TestCA" -t "Pu,Pu,Pu" -m "$CERTSERIAL" -d "${CERTDIR}" -f "${PWFILE}" -z "${NOISE_FILE}" -v 60 2>&1
if [ "$CERTFAILED" != 0 ] ; then
    certlog "ERROR: SSL failed $RET"
else
    certlog "SUCCESS: SSL passed"
fi
certlog "finished"

echo "</TABLE><BR>" >> ${RESULTS}

rm -f ${TEMPFILES}

# we will probably need mor for the tools 
# tools.sh: generates an alice cert in a "Cert" directory
# FIXME, for now use ALICEDIR and see if this works...
