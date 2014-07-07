#!/bin/bash

export NSS_DEFAULT_DB_TYPE=sql

export BASE_PATH=`dirname $0`
export SIGN_SCR_LOC=.
export APPS_TEST_LOC=../../../../../../../dom/apps/tests/signed
export TOOLKIT_WEBAPPS_TEST_LOC=../../../../../../../toolkit/webapps/tests/data/

# Creates the entry zip files (unsigned apps) from the source directories
packageApps() {
APPS="unsigned_app_1 unsigned_app_origin unsigned_app_origin_toolkit_webapps"
OLD_PWD=`pwd`
cd ${BASE_PATH}
for i in $APPS
do
  echo "Creating $i.zip"
  cd $i && zip -r ../$i.zip . && cd ..
done
cd ${OLD_PWD}
}


# Function to create a signing database
# Parameters:
# $1: Output directory (where the DB will be created)
createDb() {

  db=$1

  mkdir -p $db

  # Insecure by design, so... please don't use this for anything serious
  passwordfile=$db/passwordfile

  echo insecurepassword > $passwordfile
  certutil -d $db -N -f $passwordfile 2>&1 >/dev/null

}

# Add a CA cert and a signing cert to the database
# Arguments:
#   $1: DB directory
#   $2: CA CN (don't include the CN=, just the value)
#   $3: Signing Cert CN (don't include the CN=, just the value)
#   $4: CA short name (don't use spaces!)
#   $5: Signing Cert short name (don't use spaces!)
addCerts() {
  org="O=Examplla Corporation,L=Mountain View,ST=CA,C=US"
  ca_subj="CN=${2},${org}"
  ee_subj="CN=${3},${org}"

  noisefile=/tmp/noise.$$
  head -c 32 /dev/urandom > $noisefile

  ca_responses=/tmp/caresponses.$$
  ee_responses=/tmp/earesponses

  echo y >  $ca_responses # Is this a CA?
  echo >>   $ca_responses # Accept default path length constraint (no constraint)
  echo y >> $ca_responses # Is this a critical constraint?
  echo n >  $ee_responses # Is this a CA?
  echo >>   $ee_responses # Accept default path length constraint (no constraint)
  echo y >> $ee_responses # Is this a critical constraint?

  make_cert="certutil -d $db -f $passwordfile -S -g 2048 -Z SHA256 \
                    -z $noisefile -y 3 -2 --extKeyUsage critical,codeSigning"
  $make_cert -v 480 -n ${4}        -m 1 -s "$ca_subj" \
      --keyUsage critical,certSigning      -t ",,CTu" -x < $ca_responses 2>&1 >/dev/null
  $make_cert -v 240 -n ${5} -c ${4} -m 2 -s "$ee_subj" \
      --keyUsage critical,digitalSignature -t ",,,"      < $ee_responses 2>&1 >/dev/null

  # In case we want to inspect the generated certs

  # Also, we'll need this one later on
  certutil -d $db -L -n ${4} -r -o $db/${4}.der
  certutil -d $db -L -n ${5} -r -o $db/${5}.der

  rm -f $noisefile $ee_responses $ca_responses
}


# Signs an app
# Parameters:
# $1: Database directory
# $2: Unsigned ZIP file path
# $3: Signed ZIP file path
# $4: Store ID for the signed App
# $5: Version of the signed App
# $6: Nickname of the signing certificate
signApp() {

  db=$1

  # Once again, this is INSECURE. It doesn't matter here but
  # DON'T use this for anything production related
  passwordfile=$db/passwordfile

  python ${BASE_PATH}/${SIGN_SCR_LOC}/sign_b2g_app.py -d $db -f $passwordfile \
         -k ${6} -i ${2} -o ${3} -S ${4} -V ${5}
}

DB_PATH=${BASE_PATH}/signingDB
TEST_APP_PATH=${BASE_PATH}/testApps

echo "Warning! The directories ${DB_PATH} and ${TEST_APP_PATH} will be erased!"
echo "Do you want to proceed anyway?"
select answer in "Yes" "No"
do
  case $answer in
    Yes) break;;
    No) exit 1;;
  esac
done

rm -rf ${DB_PATH} ${TEST_APP_PATH}

TRUSTED_EE=trusted_ee1
UNTRUSTED_EE=untrusted_ee1
TRUSTED_CA=trusted_ca1
UNTRUSTED_CA=untrusted_ca1

# First, we'll create a new couple of signing DBs
createDb $DB_PATH
addCerts $DB_PATH "Valid CA" "Store Cert" trusted_ca1 ${TRUSTED_EE}
addCerts $DB_PATH "Invalid CA" "Invalid Cert" ${UNTRUSTED_CA} ${UNTRUSTED_EE}

# Then we'll create the unsigned apps
echo "Creating unsigned apps"
packageApps

# And then we'll create all the test apps...
mkdir -p ${TEST_APP_PATH}

# We need:
# A valid signed file, with two different versions:
#    valid_app_1.zip
#    valid_app_2.zip
VALID_UID=`uuidgen`
signApp $DB_PATH ${BASE_PATH}/unsigned_app_1.zip \
        $TEST_APP_PATH/valid_app_1.zip \
        $VALID_UID 1 ${TRUSTED_EE}
signApp $DB_PATH ${BASE_PATH}/unsigned_app_1.zip \
        $TEST_APP_PATH/valid_app_2.zip \
        $VALID_UID 2 ${TRUSTED_EE}


# A corrupt_package:
#    corrupt_app_1.zip
# A corrupt package is a package with a entry modified, for example...
CURDIR=`pwd`
export TEMP_DIR=$TEST_APP_PATH/aux_unzip_$$
mkdir -p $TEMP_DIR
cd  $TEMP_DIR
unzip ../valid_app_1.zip 2>&1 >/dev/null
echo " - " >> index.html
zip -r ../corrupt_app_1.zip * 2>&1 >/dev/null
cd $CURDIR
rm -rf $TEMP_DIR

# A file signed by a unknown issuer
#    unknown_issuer_app_1.zip
INVALID_UID=`uuidgen`
signApp $DB_PATH ${BASE_PATH}/unsigned_app_1.zip \
        $TEST_APP_PATH/unknown_issuer_app_1.zip \
        $INVALID_UID 1 ${UNTRUSTED_EE}

# And finally a priviledged signed file that includes the origin on the manifest
# to avoid that reverting again
PRIV_UID=`uuidgen`
signApp $DB_PATH ${BASE_PATH}/unsigned_app_origin.zip \
        $TEST_APP_PATH/origin_app_1.zip \
        $PRIV_UID 1 ${TRUSTED_EE}

# A privileged signed app needed for a toolkit/webapps test
PRIV_TOOLKIT_UID=`uuidgen`
signApp $DB_PATH ${BASE_PATH}/unsigned_app_origin_toolkit_webapps.zip \
        $TEST_APP_PATH/custom_origin.zip \
        $PRIV_TOOLKIT_UID 1 ${TRUSTED_EE}

# Now let's copy the trusted cert to the app directory so we have everything
# on the same place...
cp ${DB_PATH}/${TRUSTED_CA}.der ${TEST_APP_PATH}

cat <<EOF

All done. The new test files are in ${TEST_APP_PATH}. You should copy the
contents of that directory to the dom/apps/tests/signed directory and to
the security/manager/ssl/tests/unit/test_signed_apps (which should be the
parent of this directory) to install them.

EOF

echo "Do you wish me to do that for you now?"
select answer in "Yes" "No"
do
  case $answer in
    Yes) break;;
    No) echo "Ok, not installing the new files"
        echo "You should run: "
        echo cp ${TEST_APP_PATH}/* ${BASE_PATH}/${APPS_TEST_LOC}
        echo cp ${TEST_APP_PATH}/* ${TEST_APP_PATH}/../unsigned_app_1.zip ${BASE_PATH}/..
        echo cp ${TEST_APP_PATH}/* ${BASE_PATH}/${TOOLKIT_WEBAPPS_TEST_LOC}
        echo "to install them"
        exit 0;;
  esac
done

cp ${TEST_APP_PATH}/* ${BASE_PATH}/${APPS_TEST_LOC}
cp ${TEST_APP_PATH}/* ${TEST_APP_PATH}/../unsigned_app_1.zip ${BASE_PATH}/..
cp ${TEST_APP_PATH}/* ${BASE_PATH}/${TOOLKIT_WEBAPPS_TEST_LOC}

echo "Done!"
