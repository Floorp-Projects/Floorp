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
# mozilla/security/nss/tests/cert/rcert.sh
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
      . ./init.sh
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
    echo "$SCRIPTNAME $*"
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
    #netstat >> ${NOISE_FILE} 2>&1
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
        RET=$?
        CU_SUBJECT=""
    else
        echo "certutil $*"
        certutil $*
        RET=$?
    fi
    if [ "$RET" -ne 0 ]; then
        CERTFAILED=$RET
        html_failed "<TR><TD>${CU_ACTION} ($RET) " 
        cert_log "ERROR: ${CU_ACTION} failed $RET"
    else
        html_passed "<TR><TD>${CU_ACTION}"
    fi

    # echo "Contine?"
    # cat > /dev/null
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
    DOMAIN="$4"

    if [ ! -d "${CERTDIR}" ]; then
        mkdir -p "${CERTDIR}"
    else
        echo "$SCRIPTNAME: WARNING - ${CERTDIR} exists"
    fi
    cd "${CERTDIR}"
    CERTDIR="." 

    PROFILEDIR=${CERTDIR}
    if [ -n "${MULTIACCESS_DBM}" ]; then
	PROFILEDIR="multiaccess:${DOMAIN}"
    fi

    noise
}

############################# hw_acc #################################
# local shell function to add hw accelerator modules to the db
########################################################################
hw_acc()
{
    HW_ACC_RET=0
    HW_ACC_ERR=""
    if [ -n "$O_HWACC" -a "$O_HWACC" = ON -a -z "$USE_64" ] ; then
        echo "creating $CERTNAME s cert with hwaccelerator..."
        #case $ACCELERATOR in
        #rainbow)
   

        echo "modutil -add rainbow -libfile /usr/lib/libcryptoki22.so "
        echo "         -dbdir ${PROFILEDIR} 2>&1 "
        echo | modutil -add rainbow -libfile /usr/lib/libcryptoki22.so \
            -dbdir ${PROFILEDIR} 2>&1 
        if [ "$?" -ne 0 ]; then
            echo "modutil -add rainbow failed in `pwd`"
            HW_ACC_RET=1
            HW_ACC_ERR="modutil -add rainbow"
        fi
    
        echo "modutil -add ncipher "
        echo "         -libfile /opt/nfast/toolkits/pkcs11/libcknfast.so "
        echo "         -dbdir ${PROFILEDIR} 2>&1 "
        echo | modutil -add ncipher \
            -libfile /opt/nfast/toolkits/pkcs11/libcknfast.so \
            -dbdir ${PROFILEDIR} 2>&1 
        if [ "$?" -ne 0 ]; then
            echo "modutil -add ncipher failed in `pwd`"
            HW_ACC_RET=`expr $HW_ACC_RET + 2`
            HW_ACC_ERR="$HW_ACC_ERR,modutil -add ncipher"
        fi
        if [ "$HW_ACC_RET" -ne 0 ]; then
            html_failed "<TR><TD>Adding HW accelerators to certDB for ${CERTNAME} ($HW_ACC_RET) " 
        else
            html_passed "<TR><TD>Adding HW accelerators to certDB for ${CERTNAME}"
        fi

    fi
    return $HW_ACC_RET
}

############################# cert_create_cert #########################
# local shell function to create client certs 
#     initialize DB, import
#     root cert
#     add cert to DB
########################################################################
cert_create_cert()
{
    cert_init_cert "$1" "$2" "$3" "$4"

    CU_ACTION="Initializing ${CERTNAME}'s Cert DB"
    certu -N -d "${PROFILEDIR}" -f "${R_PWFILE}" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi
    hw_acc
    CU_ACTION="Import Root CA for $CERTNAME"
    certu -A -n "TestCA" -t "TC,TC,TC" -f "${R_PWFILE}" -d "${PROFILEDIR}" \
          -i "${R_CADIR}/root.cert" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi
    cert_add_cert "$5"
    return $?
}

############################# cert_add_cert ############################
# local shell function to add client certs to an existing CERT DB
#     generate request
#     sign request
#     import Cert
#
########################################################################
cert_add_cert()
{

    CU_ACTION="Generate Cert Request for $CERTNAME"
    CU_SUBJECT="CN=$CERTNAME, E=${CERTNAME}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    certu -R -d "${PROFILEDIR}" -f "${R_PWFILE}" -z "${R_NOISE_FILE}" -o req  2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Sign ${CERTNAME}'s Request"
    certu -C -c "TestCA" -m "$CERTSERIAL" -v 60 -d "${P_R_CADIR}" \
          -i req -o "${CERTNAME}.cert" -f "${R_PWFILE}" "$1" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    CU_ACTION="Import $CERTNAME's Cert"
    certu -A -n "$CERTNAME" -t "u,u,u" -d "${PROFILEDIR}" -f "${R_PWFILE}" \
          -i "${CERTNAME}.cert" 2>&1
    if [ "$RET" -ne 0 ]; then
        return $RET
    fi

    cert_log "SUCCESS: $CERTNAME's Cert Created"
    return 0
}

################################# cert_all_CA ################################
# local shell function to build the additional Temp. Certificate Authority (CA)
# used for the "real life" ssl test with 2 different CA's in the
# client and in teh server's dir
##########################################################################
cert_all_CA()
{
    echo nss > ${PWFILE}

    ALL_CU_SUBJECT="CN=NSS Test CA, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
    cert_CA $CADIR TestCA -x "CTu,CTu,CTu" ${D_CA} "1"

    ALL_CU_SUBJECT="CN=NSS Server Test CA, O=BOGUS NSS, L=Santa Clara, ST=California, C=US"
    cert_CA $SERVER_CADIR serverCA -x "Cu,Cu,Cu" ${D_SERVER_CA} "2"
    ALL_CU_SUBJECT="CN=NSS Chain1 Server Test CA, O=BOGUS NSS, L=Santa Clara, ST=California, C=US"
    cert_CA $SERVER_CADIR chain-1-serverCA "-c serverCA" "u,u,u" ${D_SERVER_CA} "3"
    ALL_CU_SUBJECT="CN=NSS Chain2 Server Test CA, O=BOGUS NSS, L=Santa Clara, ST=California, C=US" 
    cert_CA $SERVER_CADIR chain-2-serverCA "-c chain-1-serverCA" "u,u,u" ${D_SERVER_CA} "4"



    ALL_CU_SUBJECT="CN=NSS Client Test CA, O=BOGUS NSS, L=Santa Clara, ST=California, C=US"
    cert_CA $CLIENT_CADIR clientCA -x "Tu,Cu,Cu" ${D_CLIENT_CA} "5"
    ALL_CU_SUBJECT="CN=NSS Chain1 Client Test CA, O=BOGUS NSS, L=Santa Clara, ST=California, C=US"
    cert_CA $CLIENT_CADIR chain-1-clientCA "-c clientCA" "u,u,u" ${D_CLIENT_CA} "6"
    ALL_CU_SUBJECT="CN=NSS Chain2 Client Test CA, O=BOGUS NSS, L=Santa Clara, ST=California, C=US"
    cert_CA $CLIENT_CADIR chain-2-clientCA "-c chain-1-clientCA" "u,u,u" ${D_CLIENT_CA} "7"

    rm $CLIENT_CADIR/root.cert $SERVER_CADIR/root.cert
    # root.cert in $CLIENT_CADIR and in $SERVER_CADIR is the one of the last 
    # in the chain
}

################################# cert_CA ################################
# local shell function to build the Temp. Certificate Authority (CA)
# used for testing purposes, creating  a CA Certificate and a root cert
##########################################################################
cert_CA()
{
  CUR_CADIR=$1
  NICKNAME=$2
  SIGNER=$3
  TRUSTARG=$4
  DOMAIN=$5
  CERTSERIAL=$6

  echo "$SCRIPTNAME: Creating a CA Certificate $NICKNAME =========================="

  if [ ! -d "${CUR_CADIR}" ]; then
      mkdir -p "${CUR_CADIR}"
  fi
  cd ${CUR_CADIR}
  pwd

  LPROFILE=.
  if [ -n "${MULTIACCESS_DBM}" ]; then
	LPROFILE="multiaccess:${DOMAIN}"
  fi

  if [ "$SIGNER" = "-x" ] ; then # self signed -> create DB
      CU_ACTION="Creating CA Cert DB"
      certu -N -d ${LPROFILE} -f ${R_PWFILE} 2>&1
      if [ "$RET" -ne 0 ]; then
          Exit 5 "Fatal - failed to create CA $NICKNAME "
      fi
      echo "$SCRIPTNAME: Certificate initialized ----------"
  fi


  ################# Creating CA Cert ######################################
  #
  CU_ACTION="Creating CA Cert $NICKNAME "
  CU_SUBJECT=$ALL_CU_SUBJECT
  certu -S -n $NICKNAME -t $TRUSTARG -v 60 $SIGNER -d ${LPROFILE} -1 -2 -5 \
        -f ${R_PWFILE} -z ${R_NOISE_FILE} -m $CERTSERIAL 2>&1 <<CERTSCRIPT
5
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
CERTSCRIPT

  if [ "$RET" -ne 0 ]; then
      echo "return value is $RET"
      Exit 6 "Fatal - failed to create CA cert"
  fi

  ################# Exporting Root Cert ###################################
  #
  CU_ACTION="Exporting Root Cert"
  certu -L -n  $NICKNAME -r -d ${LPROFILE} -o root.cert 
  if [ "$RET" -ne 0 ]; then
      Exit 7 "Fatal - failed to export root cert"
  fi
  cp root.cert ${NICKNAME}.ca.cert
}

############################## cert_smime_client #############################
# local shell function to create client Certificates for S/MIME tests 
##############################################################################
cert_smime_client()
{
  CERTFAILED=0
  echo "$SCRIPTNAME: Creating Client CA Issued Certificates =============="

  cert_create_cert ${ALICEDIR} "Alice" 30 ${D_ALICE}
  cert_create_cert ${BOBDIR} "Bob" 40  ${D_BOB}

  echo "$SCRIPTNAME: Creating Dave's Certificate -------------------------"
  cert_create_cert "${DAVEDIR}" Dave 50 ${D_DAVE}

  echo "$SCRIPTNAME: Creating multiEmail's Certificate --------------------"
  cert_create_cert "${EVEDIR}" "Eve" 60 ${D_EVE} "-7 eve@bogus.net,eve@bogus.cc,beve@bogus.com"

  #echo "************* Copying CA files to ${SERVERDIR}"
  #cp ${CADIR}/*.db .
  #hw_acc

  #########################################################################
  #
  #cd ${CERTDIR}
  #CU_ACTION="Creating ${CERTNAME}'s Server Cert"
  #CU_SUBJECT="CN=${CERTNAME}, E=${CERTNAME}@bogus.com, O=BOGUS Netscape, L=Mountain View, ST=California, C=US"
  #certu -S -n "${CERTNAME}" -c "TestCA" -t "u,u,u" -m "$CERTSERIAL" \
  #	-d ${PROFILEDIR} -f "${R_PWFILE}" -z "${R_NOISE_FILE}" -v 60 2>&1

  #CU_ACTION="Export Dave's Cert"
  #cd ${DAVEDIR}
  #certu -L -n "Dave" -r -d ${P_R_DAVE} -o Dave.cert

  ################# Importing Certificates for S/MIME tests ###############
  #
  echo "$SCRIPTNAME: Importing Certificates =============================="
  CU_ACTION="Import Alices's cert into Bob's db"
  certu -E -t "p,p,p" -d ${P_R_BOBDIR} -f ${R_PWFILE} \
        -i ${R_ALICEDIR}/Alice.cert 2>&1

  CU_ACTION="Import Bob's cert into Alice's db"
  certu -E -t "p,p,p" -d ${P_R_ALICEDIR} -f ${R_PWFILE} \
        -i ${R_BOBDIR}/Bob.cert 2>&1

  CU_ACTION="Import Dave's cert into Alice's DB"
  certu -E -t "p,p,p" -d ${P_R_ALICEDIR} -f ${R_PWFILE} \
        -i ${R_DAVEDIR}/Dave.cert 2>&1

  CU_ACTION="Import Dave's cert into Bob's DB"
  certu -E -t "p,p,p" -d ${P_R_BOBDIR} -f ${R_PWFILE} \
        -i ${R_DAVEDIR}/Dave.cert 2>&1

  CU_ACTION="Import Eve's cert into Alice's DB"
  certu -E -t "p,p,p" -d ${P_R_ALICEDIR} -f ${R_PWFILE} \
        -i ${R_EVEDIR}/Eve.cert 2>&1

  CU_ACTION="Import Eve's cert into Bob's DB"
  certu -E -t "p,p,p" -d ${P_R_BOBDIR} -f ${R_PWFILE} \
        -i ${R_EVEDIR}/Eve.cert 2>&1

  if [ "$CERTFAILED" != 0 ] ; then
      cert_log "ERROR: SMIME failed $RET"
  else
      cert_log "SUCCESS: SMIME passed"
  fi
}

############################## cert_ssl ################################
# local shell function to create client + server certs for extended SSL test
########################################################################
cert_extended_ssl()
{
  ################# Creating Certs for extended SSL test ####################
  #
  CERTFAILED=0
  echo "$SCRIPTNAME: Creating Certificates, issued by the last ==============="
  echo "     of a chain of CA's which are not in the same database============"

  echo "Server Cert"
  cert_init_cert ${EXT_SERVERDIR} "${HOSTADDR}" 1 ${D_EXT_SERVER}

  CU_ACTION="Initializing ${CERTNAME}'s Cert DB (ext.)"
  certu -N -d "${PROFILEDIR}" -f "${R_PWFILE}" 2>&1

  CU_ACTION="Generate Cert Request for $CERTNAME (ext)"
  CU_SUBJECT="CN=$CERTNAME, E=${CERTNAME}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
  certu -R -d "${PROFILEDIR}" -f "${R_PWFILE}" -z "${R_NOISE_FILE}" -o req 2>&1

  CU_ACTION="Sign ${CERTNAME}'s Request (ext)"
  cp ${CERTDIR}/req ${SERVER_CADIR}
  certu -C -c "chain-2-serverCA" -m 200 -v 60 -d "${P_SERVER_CADIR}" \
        -i req -o "${CERTNAME}.cert" -f "${R_PWFILE}" 2>&1

  CU_ACTION="Import $CERTNAME's Cert  -t u,u,u (ext)"
  certu -A -n "$CERTNAME" -t "u,u,u" -d "${PROFILEDIR}" -f "${R_PWFILE}" \
        -i "${CERTNAME}.cert" 2>&1

  CU_ACTION="Import Client Root CA -t T,, for $CERTNAME (ext.)"
  certu -A -n "clientCA" -t "T,," -f "${R_PWFILE}" -d "${PROFILEDIR}" \
          -i "${CLIENT_CADIR}/clientCA.ca.cert" 2>&1
  echo "Importing all the server's own CA chain into the servers DB"
  for CA in `find ${SERVER_CADIR} -name "?*.ca.cert"` ;
  do
      N=`basename $CA | sed -e "s/.ca.cert//"`
      if [ $N = "serverCA" ] ; then
          T="-t C,C,C"
      else
          T="-t u,u,u"
      fi
      CU_ACTION="Import $N CA $T for $CERTNAME (ext.) "
      certu -A -n $N  $T -f "${R_PWFILE}" -d "${PROFILEDIR}" \
          -i "${CA}" 2>&1
  done
#============
  echo "Client Cert"
  cert_init_cert ${EXT_CLIENTDIR} ExtendedSSLUser 1 ${D_EXT_CLIENT}

  CU_ACTION="Initializing ${CERTNAME}'s Cert DB (ext.)"
  certu -N -d "${PROFILEDIR}" -f "${R_PWFILE}" 2>&1

  CU_ACTION="Generate Cert Request for $CERTNAME (ext)"
  CU_SUBJECT="CN=$CERTNAME, E=${CERTNAME}@bogus.com, O=BOGUS NSS, L=Mountain View, ST=California, C=US"
  certu -R -d "${PROFILEDIR}" -f "${R_PWFILE}" -z "${R_NOISE_FILE}" -o req 2>&1

  CU_ACTION="Sign ${CERTNAME}'s Request (ext)"
  cp ${CERTDIR}/req ${CLIENT_CADIR}
  certu -C -c "chain-2-clientCA" -m 300 -v 60 -d "${P_CLIENT_CADIR}" \
        -i req -o "${CERTNAME}.cert" -f "${R_PWFILE}" 2>&1

  CU_ACTION="Import $CERTNAME's Cert -t u,u,u (ext)"
  certu -A -n "$CERTNAME" -t "u,u,u" -d "${PROFILEDIR}" -f "${R_PWFILE}" \
        -i "${CERTNAME}.cert" 2>&1
  CU_ACTION="Import Server Root CA -t C,C,C for $CERTNAME (ext.)"
  certu -A -n "serverCA" -t "C,C,C" -f "${R_PWFILE}" -d "${PROFILEDIR}" \
          -i "${SERVER_CADIR}/serverCA.ca.cert" 2>&1
  echo "Importing all the client's own CA chain into the servers DB"
  for CA in `find ${CLIENT_CADIR} -name "?*.ca.cert"` ;
  do
      N=`basename $CA | sed -e "s/.ca.cert//"`
      if [ $N = "clientCA" ] ; then
          T="-t T,C,C"
      else
          T="-t u,u,u"
      fi
      CU_ACTION="Import $N CA $T for $CERTNAME (ext.)"
      certu -A -n $N  $T -f "${R_PWFILE}" -d "${PROFILEDIR}" \
          -i "${CA}" 2>&1
  done
  if [ "$CERTFAILED" != 0 ] ; then
      cert_log "ERROR: EXT failed $RET"
  else
      cert_log "SUCCESS: EXT passed"
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
  cert_create_cert ${CLIENTDIR} "TestUser" 70 ${D_CLIENT}

  echo "$SCRIPTNAME: Creating Server CA Issued Certificate for \\"
  echo "             ${HOSTADDR} ------------------------------------"
  cert_create_cert ${SERVERDIR} "${HOSTADDR}" 100 ${D_SERVER}
  certu -M -n "TestCA" -t "TC,TC,TC" -d ${PROFILEDIR}
#  cert_init_cert ${SERVERDIR} "${HOSTADDR}" 1 ${D_SERVER}
#  echo "************* Copying CA files to ${SERVERDIR}"
#  cp ${CADIR}/*.db .
#  hw_acc
#  CU_ACTION="Creating ${CERTNAME}'s Server Cert"
#  CU_SUBJECT="CN=${CERTNAME}, O=BOGUS Netscape, L=Mountain View, ST=California, C=US"
#  certu -S -n "${CERTNAME}" -c "TestCA" -t "Pu,Pu,Pu" -d ${PROFILEDIR} \
#	 -f "${R_PWFILE}" -z "${R_NOISE_FILE}" -v 60 2>&1

  if [ "$CERTFAILED" != 0 ] ; then
      cert_log "ERROR: SSL failed $RET"
  else
      cert_log "SUCCESS: SSL passed"
  fi
}
############################## cert_stresscerts ################################
# local shell function to create client certs for SSL stresstest
########################################################################
cert_stresscerts()
{

  ############### Creating Certs for SSL stress test #######################
  #
  CERTDIR="$CLIENTDIR"
  cd "${CERTDIR}"

  PROFILEDIR=${CERTDIR}
  if [ -n "${MULTIACCESS_DBM}" ]; then
     PROFILEDIR="multiaccess:${D_CLIENT}"
  fi
  CERTFAILED=0
  echo "$SCRIPTNAME: Creating Client CA Issued Certificates ==============="

  CONTINUE=$GLOB_MAX_CERT
  CERTSERIAL=10

  while [ $CONTINUE -ge $GLOB_MIN_CERT ]
  do
      CERTNAME="TestUser$CONTINUE"
#      cert_add_cert ${CLIENTDIR} "TestUser$CONTINUE" $CERTSERIAL
      cert_add_cert 
      CERTSERIAL=`expr $CERTSERIAL + 1 `
      CONTINUE=`expr $CONTINUE - 1 `
  done
  if [ "$CERTFAILED" != 0 ] ; then
      cert_log "ERROR: StressCert failed $RET"
  else
      cert_log "SUCCESS: StressCert passed"
  fi
}

############################## cert_fips #####################################
# local shell function to create certificates for FIPS tests 
##############################################################################
cert_fips()
{
  CERTFAILED=0
  echo "$SCRIPTNAME: Creating FIPS 140-1 DSA Certificates =============="
  cert_init_cert "${FIPSDIR}" "FIPS PUB 140-1 Test Certificate" 1000 "${D_FIPS}"

  CU_ACTION="Initializing ${CERTNAME}'s Cert DB"
  certu -N -d "${PROFILEDIR}" -f "${R_FIPSPWFILE}" 2>&1

  echo "$SCRIPTNAME: Enable FIPS mode on database -----------------------"
  modutil -dbdir ${PROFILEDIR} -fips true 2>&1 <<MODSCRIPT
y
MODSCRIPT
  CU_ACTION="Enable FIPS mode on database for ${CERTNAME}"
  if [ "$?" -ne 0 ]; then
    html_failed "<TR><TD>${CU_ACTION} ($?) " 
    cert_log "ERROR: ${CU_ACTION} failed $?"
  else
    html_passed "<TR><TD>${CU_ACTION}"
  fi

  CU_ACTION="Generate Certificate for ${CERTNAME}"
  CU_SUBJECT="CN=${CERTNAME}, E=fips@bogus.com, O=BOGUS NSS, OU=FIPS PUB 140-1, L=Mountain View, ST=California, C=US"
  certu -S -n ${FIPSCERTNICK} -x -t "Cu,Cu,Cu" -d "${PROFILEDIR}" -f "${R_FIPSPWFILE}" -k dsa -m 500 -z "${R_NOISE_FILE}" 2>&1
  if [ "$RET" -eq 0 ]; then
    cert_log "SUCCESS: FIPS passed"
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
cert_all_CA
cert_extended_ssl 
cert_ssl 
cert_smime_client        
cert_fips
if [ -n "$DO_DIST_ST" -a "$DO_DIST_ST" = "TRUE" ] ; then
    cert_stresscerts 
    #following lines to be used when databases are to be reused
    #cp -r /u/sonmi/tmp/stress/kentuckyderby.13/* $HOSTDIR
    #cp -r $HOSTDIR/../${HOST}.2/* $HOSTDIR

fi
cert_cleanup
