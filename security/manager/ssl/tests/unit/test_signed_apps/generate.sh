#!/bin/bash
# Usage:
# export NSS_PREFIX=<path to NSS tools> \
# PATH=$NSS_PREFIX/bin:$NSS_PREFIX/lib:$PATH ./generate.sh

set -e

srcdir=$PWD
tmpdir=$TMP/test_signed_apps
noisefile=$tmpdir/noise
passwordfile=$tmpdir/passwordfile
ca_responses=$tmpdir/ca_responses
ee_responses=$tmpdir/ee_responses

replace_zip()
{
  zip=$1 # must be an absolute path
  new_contents_dir=$2

  rm -f $zip
  oldpwd=$PWD
  cd $new_contents_dir && zip -9 -o -r $zip *
  cd $oldpwd
}

sign_app_with_new_cert()
{
  label=$1
  unsigned_zip=$2
  out_signed_zip=$3

  # XXX: We cannot give the trusted and untrusted versions of the certs the same
  # subject names because otherwise we'll run into
  # SEC_ERROR_REUSED_ISSUER_AND_SERIAL.
  org="O=Examplla Corporation,L=Mountain View,ST=CA,C=US"
  ca_subj="CN=Examplla Root CA $label,OU=Examplla CA,$org"
  ee_subj="CN=Examplla Marketplace App Signing $label,OU=Examplla Marketplace App Signing,$org"

  db=$tmpdir/$label
  mkdir -p $db
  certutil -d $db -N -f $passwordfile
  make_cert="certutil -d $db -f $passwordfile -S -v 3 -g 2048 -Z SHA256 \
                      -z $noisefile -y 3 -2 --extKeyUsage critical,codeSigning"
  $make_cert -n ca1        -m 1 -s "$ca_subj" \
             --keyUsage critical,certSigning      -t ",,CTu" -x < $ca_responses
  $make_cert -n ee1 -c ca1 -m 2 -s "$ee_subj" \
             --keyUsage critical,digitalSignature -t ",,,"      < $ee_responses

  # In case we want to inspect the generated certs
  certutil -d $db -L -n ca1 -r -o $db/ca1.der
  certutil -d $db -L -n ee1 -r -o $db/ee1.der

  python sign_b2g_app.py -d $db -f $passwordfile -k ee1 -i $unsigned_zip -o $out_signed_zip
}

rm -Rf $tmpdir
mkdir $tmpdir

echo password1 > $passwordfile
head --bytes 32 /dev/urandom > $noisefile

# XXX: certutil cannot generate basic constraints without interactive prompts,
#      so we need to build response files to answer its questions
# XXX: certutil cannot generate AKI/SKI without interactive prompts so we just
#      skip them.
echo y >  $ca_responses # Is this a CA?
echo >>   $ca_responses # Accept default path length constraint (no constraint)
echo y >> $ca_responses # Is this a critical constraint?
echo n >  $ee_responses # Is this a CA?
echo >>   $ee_responses # Accept default path length constraint (no constraint)
echo y >> $ee_responses # Is this a critical constraint?

replace_zip $srcdir/unsigned.zip $srcdir/simple

sign_app_with_new_cert trusted   $srcdir/unsigned.zip $srcdir/valid.zip
sign_app_with_new_cert untrusted $srcdir/unsigned.zip $srcdir/unknown_issuer.zip
certutil -d $tmpdir/trusted -f $passwordfile -L -n ca1 -r -o $srcdir/trusted_ca1.der

rm -Rf $tmpdir
