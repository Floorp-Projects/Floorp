#! /bin/sh
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

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

############################## cert_init ###############################
# local shell function to initialize this script
########################################################################
cert_init()
{
  SCRIPTNAME="cert.sh"
  if [ -z "${CLEANUP}" ] ; then     # if nobody else is responsible for
      CLEANUP="${SCRIPTNAME}"       # cleaning this script will do it
  fi
  if [ -z "${INIT_SOURCED}" ] ; then
      cd ../common
      . init.sh
  fi
  SCRIPTNAME="cert.sh"
  html_head "Certutil Tests"

  ################## Generate noise for our CA cert. ######################
  # NOTE: these keys are only suitable for testing, as this whole thing 
  # bypasses the entropy gathering. Don't use this method to generate 
  # keys and certs for product use or deployment.
  #
  ps -efl > ${NOISE_FILE} 2>&1
  ps aux >> ${NOISE_FILE} 2>&1
  noise
}

cert_log() ######################    write the cert_status file
{
    #echo "$SCRIPTNAME $*"
    echo $* >>${CERT_LOG_FILE}
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
    echo "$SCRIPTNAME: ${CU_ACTION} --------------------------"

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
        html_failed "<TR><TD>${CU_ACTION} ($RET) " 
        cert_log "ERROR: ${CU_ACTION} failed $RET"
    else
        html_passed "<TR><TD>${CU_ACTION}"
    fi
    return $RET
}

############################# cert_init_cert ##########################
# local shell function to initialize creation of client and server certs
########################################################################
cert_init_cert()
{
    CERTDIR="$1"
    CERTNAME="$2"
    CERTSERIAL="$3"

    if [ ! -d "${CERTDIR}" ]; then
        mkdir -p "${CERTDIR}"
    else
        echo "$SCRIPTNAME: WARNING - ${CERTDIR} exists"
    fi
    cd "${CERTDIR}"
    CERTDIR="." 

    noise
}

############################# cert_create_cert #########################
# local shell function to create client certs 
#     initialize DB, import
#     root cert
#     generate request
#     sign request
#     import Cert
#
########################################################################
cert_create_cert()
{
    cert_init_cert "$1" "$2" "$3"

    CU_ACTION="Initializing ${CERTNAME}'s Cert DB"
    certu -N -d "${CERTDIR}" -f "${R_PWFILE}" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Import Root CA for $CERTNAME"
    certu -A -n "TestCA" -t "TC,TC,TC" -f "${R_PWFILE}" -d "${CERTDIR}" \
          -i "${R_CADIR}/root.cert" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Generate Cert Request for $CERTNAME"
    CU_SUBJECT="CN=$CERTNAME, E=${CERTNAME}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    certu -R -d "${CERTDIR}" -f "${R_PWFILE}" -z "${R_NOISE_FILE}" -o req 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Sign ${CERTNAME}'s Request"
    certu -C -c "TestCA" -m "$CERTSERIAL" -v 60 -d "${R_CADIR}" \
          -i req -o "${CERTNAME}.cert" -f "${R_PWFILE}" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Import $CERTNAME's Cert"
    certu -A -n "$CERTNAME" -t "u,u,u" -d "${CERTDIR}" -f "${R_PWFILE}" \
          -i "${CERTNAME}.cert" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    cert_log "SUCCESS: $CERTNAME's Cert Created"
    return 0
}

################################# cert_CA ################################
# local shell function to build the Temp. Certificate Authority (CA)
# used for testing purposes, creating  a CA Certificate and a root cert
##########################################################################
cert_CA()
{
  echo "$SCRIPTNAME: Creating a CA Certificate =========================="

  if [ ! -d "${CADIR}" ]; then
      mkdir -p "${CADIR}"
  fi
  cd ${CADIR}

  echo nss > ${PWFILE}

  CU_ACTION="Creating CA Cert DB"
  certu -N -d . -f ${R_PWFILE} 2>&1
  if [ "$RET" -ne 0 ]; then
      Exit 5 "Fatal - failed to create CA"
  fi

  ################# Generating Certscript #################################
  #
  echo "$SCRIPTNAME: Certificate initialized ----------"

  ################# Creating CA Cert ######################################
  #
  CU_ACTION="Creating CA Cert"
  CU_SUBJECT="CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
  certu -S -n "TestCA" -t "CTu,CTu,CTu" -v 60 -x -d . -1 -2 -5 \
        -f ${R_PWFILE} -z ${R_NOISE_FILE} 2>&1 <<CERTSCRIPT
5
9
n
y
3
n
5
6
7
9
n
CERTSCRIPT

  if [ "$RET" -ne 0 ]; then
      Exit 6 "Fatal - failed to create CA cert"
  fi

  ################# Exporting Root Cert ###################################
  #
  CU_ACTION="Exporting Root Cert"
  certu -L -n "TestCA" -r -d . -o root.cert 
  if [ "$RET" -ne 0 ]; then
      Exit 7 "Fatal - failed to export root cert"
  fi
}

############################## cert_smime_client #############################
# local shell function to create client Certificates for S/MIME tests 
##############################################################################
cert_smime_client()
{
  CERTFAILED=0
  echo "$SCRIPTNAME: Creating Client CA Issued Certificates =============="

  cert_create_cert ${ALICEDIR} "Alice" 3
  cert_create_cert ${BOBDIR} "Bob" 4

  echo "$SCRIPTNAME: Creating Dave's Certificate -------------------------"
  cert_init_cert "${DAVEDIR}" Dave 5
  cp ${CADIR}/*.db .

  #########################################################################
  #
  cd ${CERTDIR}
  CU_ACTION="Creating ${CERTNAME}'s Server Cert"
  CU_SUBJECT="CN=${CERTNAME}, E=${CERTNAME}@bogus.com, O=BOGUS Netscape, L=Mountain View, ST=California, C=US"
  certu -S -n "${CERTNAME}" -c "TestCA" -t "u,u,u" -m "$CERTSERIAL" -d . \
        -f "${R_PWFILE}" -z "${R_NOISE_FILE}" -v 60 2>&1

  CU_ACTION="Export Dave's Cert"
  cd ${DAVEDIR}
  certu -L -n "Dave" -r -d . -o Dave.cert

  ################# Importing Certificates for S/MIME tests ###############
  #
  echo "$SCRIPTNAME: Importing Certificates =============================="
  CU_ACTION="Import Alices's cert into Bob's db"
  certu -E -t "u,u,u" -d ${R_BOBDIR} -f ${R_PWFILE} \
        -i ${R_ALICEDIR}/Alice.cert 2>&1

  CU_ACTION="Import Bob's cert into Alice's db"
  certu -E -t "u,u,u" -d ${R_ALICEDIR} -f ${R_PWFILE} \
        -i ${R_BOBDIR}/Bob.cert 2>&1

  CU_ACTION="Import Dave's cert into Alice's DB"
  certu -E -t "u,u,u" -d ${R_ALICEDIR} -f ${R_PWFILE} \
        -i ${R_DAVEDIR}/Dave.cert 2>&1

  CU_ACTION="Import Dave's cert into Bob's DB"
  certu -E -t "u,u,u" -d ${R_BOBDIR} -f ${R_PWFILE} \
        -i ${R_DAVEDIR}/Dave.cert 2>&1

  if [ "$CERTFAILED" != 0 ] ; then
      cert_log "ERROR: SMIME failed $RET"
  else
      cert_log "SUCCESS: SMIME passed"
  fi
}

############################## cert_ssl ################################
# local shell function to create client + server certs for SSL test
########################################################################
cert_ssl()
{
  ################# Creating Certs for SSL test ###########################
  #
  CERTFAILED=0
  echo "$SCRIPTNAME: Creating Client CA Issued Certificates ==============="
  cert_create_cert ${CLIENTDIR} "TestUser" 6

  echo "$SCRIPTNAME: Creating Server CA Issued Certificate for \\"
  echo "             ${HOST}.${DOMSUF} ------------------------------------"
  cert_init_cert ${SERVERDIR} "${HOST}.${DOMSUF}" 1
  cp ${CADIR}/*.db .
  CU_ACTION="Creating ${CERTNAME}'s Server Cert"
  CU_SUBJECT="CN=${CERTNAME}, O=BOGUS Netscape, L=Mountain View, ST=California, C=US"
  certu -S -n "${CERTNAME}" -c "TestCA" -t "Pu,Pu,Pu" -d . -f "${R_PWFILE}" \
        -z "${R_NOISE_FILE}" -v 60 2>&1

  #FIXME - certdir or serverdir????
  #certu -S -n "${CERTNAME}" -c "TestCA" -t "Pu,Pu,Pu" -m "$CERTSERIAL" \
  #      -d "${CERTDIR}" -f "${R_PWFILE}" -z "${R_NOISE_FILE}" -v 60 2>&1

  if [ "$CERTFAILED" != 0 ] ; then
      cert_log "ERROR: SSL failed $RET"
  else
      cert_log "SUCCESS: SSL passed"
  fi
}

############################## cert_cleanup ############################
# local shell function to finish this script (no exit since it might be
# sourced)
########################################################################
cert_cleanup()
{
  cert_log "$SCRIPTNAME: finished $SCRIPTNAME"
  html "</TABLE><BR>" 
  cd ${QADIR}
  . common/cleanup.sh
}

################## main #################################################

cert_init
cert_CA
cert_smime_client
cert_ssl
cert_cleanup
