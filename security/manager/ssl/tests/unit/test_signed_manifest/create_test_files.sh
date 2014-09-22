#!/bin/bash
#
# Mode: shell-script; sh-indentation:2;
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

export BASE_PATH=`dirname $0`
echo $BASE_PATH

# location of the 'sign_b2g_manifest.py' script
export SIGN_SCR_PATH=.

DB_PATH=${BASE_PATH}/signingDB
PASSWORD_FILE=${DB_PATH}/passwordfile
VALID_MANIFEST_PATH=${BASE_PATH}/testValidSignedManifest
INVALID_MANIFEST_PATH=${BASE_PATH}/testInvalidSignedManifest

TRUSTED_EE=trusted_ee1
UNTRUSTED_EE=untrusted_ee1
TRUSTED_CA=trusted_ca1
UNTRUSTED_CA=untrusted_ca1

# Print usage info
usage() {
    echo
    echo
    tput bold
    echo "NAME"
    tput sgr0
    echo "     create_test_files.sh - Signing a manifest for Trusted Hosted Apps."
    echo
    tput bold
    echo "SYNOPSIS"
    tput sgr0
    echo "     create_test_files.sh"
    echo "     create_test_files.sh [--regenerate-test-certs]"
    echo "     create_test_files.sh [--help]"
    echo
    tput bold
    echo "DESCRIPTION"
    tput sgr0
    echo "     The script signs a manifest for Trusted Hosted Apps if no parameter"
    echo "     is given and if the manifest file and a certificate database directory"
    echo "     is present in the current directory."
    echo "     Two directories ./testValidSignedManifest and ./testInvalidSignedManifest"
    echo "     are generated containing a manifest signature file each, signed with valid"
    echo "     and invalid certificates respectively."
    echo "     If the --regenerate-test-certs parameter is given, a new certificate database"
    echo "     directory is generated before the signing takes place."
    echo "     If the certificate database is not present and the --regenerate-test-certs"
    echo "     parameter is not given the script exits whithout any operations."
    echo
    tput bold
    echo "OPTIONS"
    echo "    --regenerate-test-certs,"
    tput sgr0
    echo "        Generates a test certificate database and then signs the manifest.webapp"
    echo "        file in the current directory."
    echo
    tput bold
    echo "    --help,"
    tput sgr0
    echo "        Show this usage information."
    echo
}

# Function to create a signing database
# Parameters:
# $1: Output directory (where the DB will be created)
# $2: Password file
createDB() {
  local db_path=${1}
  local password_file=${2}

  mkdir -p ${db_path}
  echo insecurepassword > ${password_file}
  certutil -d ${db_path} -N -f ${password_file} 2>&1 >/dev/null
}

# Add a CA cert and a signing cert to the database
# Arguments:
#   $1: DB directory
#   $2: CA CN (don't include the CN=, just the value)
#   $3: Signing Cert CN (don't include the CN=, just the value)
#   $4: CA short name (don't use spaces!)
#   $5: Signing Cert short name (don't use spaces!)
#   $6: Password file
addCerts() {
  local db_path=${1}
  local password_file=${6}

  org="O=Example Trusted Corporation,L=Mountain View,ST=CA,C=US"
  ca_subj="CN=${2},${org}"
  ee_subj="CN=${3},${org}"

  noisefile=/tmp/noise.$$
  head -c 32 /dev/urandom > ${noisefile}

  ca_responses=/tmp/caresponses.$$
  ee_responses=/tmp/earesponses

  echo y >  ${ca_responses} # Is this a CA?
  echo >>   ${ca_responses} # Accept default path length constraint (no constraint)
  echo y >> ${ca_responses} # Is this a critical constraint?
  echo n >  ${ee_responses} # Is this a CA?
  echo >>   ${ee_responses} # Accept default path length constraint (no constraint)
  echo y >> ${ee_responses} # Is this a critical constraint?

  make_cert="certutil -d ${db_path} -f ${password_file} -S -g 2048 -Z SHA256 \
                      -z ${noisefile} -y 3 -2 --extKeyUsage critical,codeSigning"
  ${make_cert} -v 480 -n ${4} -m 1 -s "${ca_subj}" --keyUsage critical,certSigning \
             -t ",,CTu" -x < ${ca_responses} 2>&1 >/dev/null
  ${make_cert} -v 240 -n ${5} -c ${4} -m 2 -s "${ee_subj}" --keyUsage critical,digitalSignature \
             -t ",,," < ${ee_responses} 2>&1 >/dev/null

  certutil -d ${db_path} -L -n ${4} -r -o ${SIGN_SCR_PATH}/${4}.der

  rm -f ${noisefile} ${ee_responses} ${ca_responses}
}

# Signs a manifest
# Parameters:
# $1: Database directory
# $2: Unsigned manifest file path
# $3: Signed manifest file path
# $4: Nickname of the signing certificate
# $5: Password file
signManifest() {
  local db_path=${1}
  local password_file=${5}

  python ${BASE_PATH}/${SIGN_SCR_PATH}/sign_b2g_manifest.py -d ${db_path} \
      -f ${password_file} -k ${4} -i ${2} -o ${3}
}

# Generate the necessary files to be used for the signing
generate_files() {
  # First create a new couple of signing DBs
  rm -rf ${DB_PATH} ${VALID_MANIFEST_PATH} ${INVALID_MANIFEST_PATH}
  createDB ${DB_PATH} ${PASSWORD_FILE}
  addCerts ${DB_PATH} "Trusted Valid CA" "Trusted Corp Cert" ${TRUSTED_CA} ${TRUSTED_EE} ${PASSWORD_FILE}
  addCerts ${DB_PATH} "Trusted Invalid CA" "Trusted Invalid Cert" ${UNTRUSTED_CA} ${UNTRUSTED_EE} ${PASSWORD_FILE}
}

#Start of execution
if [ ${1} ] && [ "${1}" == '--regenerate-test-certs' ]; then
  generate_files
elif [ "${1}" == '--help' ]; then
    usage
    exit 1
else
  if [ -d ${DB_PATH} ]; then
    rm -rf ${VALID_MANIFEST_PATH} ${INVALID_MANIFEST_PATH}
  else
    echo "Error! The directory ${DB_PATH} does not exist!"
    echo "New certificate database must be created!"
    usage $0
    exit 1
  fi
fi

# Create all the test manifests
mkdir -p ${VALID_MANIFEST_PATH}
mkdir -p ${INVALID_MANIFEST_PATH}

CURDIR=`pwd`
cd $CURDIR

# Sign a manifest file with a known issuer
signManifest ${DB_PATH} ${BASE_PATH}/manifest.webapp \
             ${VALID_MANIFEST_PATH}/manifest.sig \
             ${TRUSTED_EE} ${PASSWORD_FILE}

# Sign a manifest file with a unknown issuer
signManifest ${DB_PATH} ${BASE_PATH}/manifest.webapp \
             ${INVALID_MANIFEST_PATH}/manifest.sig \
             ${UNTRUSTED_EE} ${PASSWORD_FILE}

echo "Done!"
