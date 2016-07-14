# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
from base64 import b64encode
from hashlib import sha1
import sys
import ctypes
import nss_ctypes

def nss_create_detached_signature(cert, dataToSign, wincx):
  certdb = nss_ctypes.CERT_GetDefaultCertDB()
  p7 = nss_ctypes.SEC_PKCS7CreateSignedData(cert,
                                            nss_ctypes.certUsageObjectSigner,
                                            certdb,
                                            nss_ctypes.SEC_OID_SHA1,
                                            sha1(dataToSign).digest(),
                                            wincx)
  try:
    nss_ctypes.SEC_PKCS7AddSigningTime(p7)
    nss_ctypes.SEC_PKCS7IncludeCertChain(p7, wincx)
    return nss_ctypes.SEC_PKCS7Encode(p7, None, wincx)
  finally:
    nss_ctypes.SEC_PKCS7DestroyContentInfo(p7)

# Sign a manifest file
def sign_file(in_file, out_file, cert, wincx):
  contents = in_file.read()
  in_file.close()

  # generate base64 encoded string of the sha1 digest of the input file
  in_file_hash = b64encode(sha1(contents).digest())

  # sign the base64 encoded string with the given certificate
  in_file_signature = nss_create_detached_signature(cert, in_file_hash, wincx)

  # write the content of the output file
  out_file.write(in_file_signature)
  out_file.close()

def main():
  parser = argparse.ArgumentParser(description='Sign a B2G app manifest.')
  parser.add_argument('-d', action='store',
                            required=True, help='NSS database directory')
  parser.add_argument('-f', action='store',
                            type=argparse.FileType('rb'),
                            required=True, help='password file')
  parser.add_argument('-k', action='store',
                            required=True, help="nickname of signing cert.")
  parser.add_argument('-i', action='store', type=argparse.FileType('rb'),
                            required=True, help="input manifest file (unsigned)")
  parser.add_argument('-o', action='store', type=argparse.FileType('wb'),
                            required=True, help="output manifest file (signed)")
  args = parser.parse_args()

  db_dir = args.d
  password = args.f.readline().strip()
  cert_nickname = args.k
  cert = None

  try:
    nss_ctypes.NSS_Init(db_dir)
    wincx = nss_ctypes.SetPasswordContext(password)
    cert = nss_ctypes.PK11_FindCertFromNickname(cert_nickname, wincx)
    sign_file(args.i, args.o, cert, wincx)
    return 0
  finally:
    nss_ctypes.CERT_DestroyCertificate(cert)
    nss_ctypes.NSS_Shutdown()

if __name__ == "__main__":
    sys.exit(main())
