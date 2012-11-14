import argparse
from base64 import b64encode
from hashlib import sha1
import sys
import zipfile
import ctypes

import nss_ctypes

def nss_load_cert(nss_db_dir, nss_password, cert_nickname):
  nss_ctypes.NSS_Init(nss_db_dir)
  try:
    wincx = nss_ctypes.SetPasswordContext(nss_password)
    cert = nss_ctypes.PK11_FindCertFromNickname(cert_nickname, wincx)
    return (wincx, cert)
  except:
    nss_ctypes.NSS_Shutdown()
    raise

def nss_create_detached_signature(cert, dataToSign, wincx):
  certdb = nss_ctypes.CERT_GetDefaultCertDB()
  p7 = nss_ctypes.SEC_PKCS7CreateSignedData(cert,
                                            nss_ctypes.certUsageObjectSigner,
                                            certdb,
                                            nss_ctypes.SEC_OID_SHA1,
                                            sha1(dataToSign).digest(),
                                            wincx       )
  try:
    nss_ctypes.SEC_PKCS7AddSigningTime(p7)
    nss_ctypes.SEC_PKCS7IncludeCertChain(p7, wincx)
    return nss_ctypes.SEC_PKCS7Encode(p7, None, wincx)
  finally:
    nss_ctypes.SEC_PKCS7DestroyContentInfo(p7)

def sign_zip(in_zipfile_name, out_zipfile_name, cert, wincx):
  mf_entries = []
  seen_entries = set()

  # Change the limits in JarSignatureVerification.cpp when you change the limits
  # here.
  max_entry_uncompressed_len = 100 * 1024 * 1024
  max_total_uncompressed_len = 500 * 1024 * 1024
  max_entry_count = 100 * 1000
  max_entry_filename_len = 1024
  max_mf_len = max_entry_count * 50
  max_sf_len = 1024

  total_uncompressed_len = 0
  entry_count = 0
  with zipfile.ZipFile(out_zipfile_name, 'w') as out_zip:
    with zipfile.ZipFile(in_zipfile_name, 'r') as in_zip:
      for entry_info in in_zip.infolist():
        name = entry_info.filename

        # Check for reserved and/or insane (potentially malicious) names
        if name.endswith("/"):
          pass
          # Do nothing; we don't copy directory entries since they are just a
          # waste of space.
        elif name.lower().startswith("meta-inf/"):
          # META-INF/* is reserved for our use
          raise ValueError("META-INF entries are not allowed: %s" % (name))
        elif len(name) > max_entry_filename_len:
          raise ValueError("Entry's filename is too long: %s" % (name))
        # TODO: elif name has invalid characters...
        elif name in seen_entries:
          # It is possible for a zipfile to have duplicate entries (with the exact
          # same filenames). Python's zipfile module accepts them, but our zip
          # reader in Gecko cannot do anything useful with them, and there's no
          # sane reason for duplicate entries to exist, so reject them.
          raise ValueError("Duplicate entry in input file: %s" % (name))
        else:
          entry_count += 1
          if entry_count > max_entry_count:
            raise ValueError("Too many entries in input archive")

          seen_entries.add(name)

          # Read in the input entry, but be careful to avoid going over the
          # various limits we have, to minimize the likelihood that we'll run
          # out of memory. Note that we can't use the length from entry_info
          # because that might not be accurate if the input zip file is
          # maliciously crafted to contain misleading metadata.
          with in_zip.open(name, 'r') as entry_file:
            contents = entry_file.read(max_entry_uncompressed_len + 1)
          if len(contents) > max_entry_uncompressed_len:
            raise ValueError("Entry is too large: %s" % (name))
          total_uncompressed_len += len(contents)
          if total_uncompressed_len > max_total_uncompressed_len:
            raise ValueError("Input archive is too large")

          # Copy the entry, using the same compression as used in the input file
          out_zip.writestr(entry_info, contents)

          # Add the entry to the manifest we're building
          mf_entries.append('Name: %s\nSHA1-Digest: %s\n'
                                % (name, b64encode(sha1(contents).digest())))

    mf_contents = 'Manifest-Version: 1.0\n\n' + '\n'.join(mf_entries)
    if len(mf_contents) > max_mf_len:
      raise ValueError("Generated MANIFEST.MF is too large: %d" % (len(mf_contents)))

    sf_contents = ('Signature-Version: 1.0\nSHA1-Digest-Manifest: %s\n'
                                % (b64encode(sha1(mf_contents).digest())))
    if len(sf_contents) > max_sf_len:
      raise ValueError("Generated SIGNATURE.SF is too large: %d"
                          % (len(mf_contents)))

    p7 = nss_create_detached_signature(cert, sf_contents, wincx)

    # write the signature, SF, and MF
    out_zip.writestr("META-INF/A.RSA", p7, zipfile.ZIP_DEFLATED)
    out_zip.writestr("META-INF/A.SF",  sf_contents, zipfile.ZIP_DEFLATED)
    out_zip.writestr("META-INF/MANIFEST.MF", mf_contents, zipfile.ZIP_DEFLATED)

def main():
  parser = argparse.ArgumentParser(description='Sign a B2G app.')
  parser.add_argument('-d', action='store',
                            required=True, help='NSS database directory')
  parser.add_argument('-f', action='store',
                            type=argparse.FileType('rb'),
                            required=True, help='password file')
  parser.add_argument('-k', action='store',
                            required=True, help="nickname of signing cert.")
  parser.add_argument('-i', action='store', type=argparse.FileType('rb'),
                            required=True, help="input JAR file (unsigned)")
  parser.add_argument('-o', action='store', type=argparse.FileType('wb'),
                            required=True, help="output JAR file (signed)")
  args = parser.parse_args()

  db_dir = args.d
  password = args.f.readline().strip()
  cert_nickname = args.k

  (wincx, cert) = nss_load_cert(db_dir, password, cert_nickname)
  try:
    sign_zip(args.i, args.o, cert, wincx)
    return 0
  finally:
    nss_ctypes.CERT_DestroyCertificate(cert)
    nss_ctypes.NSS_Shutdown()

if __name__ == "__main__":
    sys.exit(main())
