#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import tempfile, os, sys
import random
import pexpect
import subprocess
import shutil

libpath = os.path.abspath('../psm_common_py')

sys.path.append(libpath)

import CertUtils

srcdir = os.getcwd()
db = tempfile.mkdtemp()

CA_basic_constraints = "basicConstraints = critical, CA:TRUE\n"
EE_basic_constraints = "basicConstraints = CA:FALSE\n"

CA_full_ku = ("keyUsage = digitalSignature, nonRepudiation, keyEncipherment, " +
              "dataEncipherment, keyAgreement, keyCertSign, cRLSign\n")

CA_eku = ("extendedKeyUsage = critical, serverAuth, clientAuth, " +
          "emailProtection, codeSigning\n")

authority_key_ident = "authorityKeyIdentifier = keyid, issuer\n"
subject_key_ident = "subjectKeyIdentifier = hash\n"


def self_sign_csr(db_dir, dst_dir, csr_name, key_file, serial_num, ext_text,
                  out_prefix):
    extensions_filename = db_dir + "/openssl-exts"
    f = open(extensions_filename, 'w')
    f.write(ext_text)
    f.close()
    cert_name = dst_dir + "/" + out_prefix + ".der"
    os.system ("openssl x509 -req -sha256 -days 3650 -in " + csr_name +
               " -signkey " + key_file +
               " -set_serial " + str(serial_num) +
               " -extfile " + extensions_filename +
               " -outform DER -out " + cert_name)



def generate_certs():
    key_type = 'rsa'
    ca_ext = CA_basic_constraints + CA_full_ku + subject_key_ident + CA_eku;
    ee_ext_text = (EE_basic_constraints + authority_key_ident)
    [ca_key, ca_cert] = CertUtils.generate_cert_generic(db,
                                                        srcdir,
                                                        1,
                                                        key_type,
                                                        'ca',
                                                        ca_ext)
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    100,
                                    key_type,
                                    'ee',
                                    ee_ext_text,
                                    ca_key,
                                    ca_cert)

    shutil.copy(ca_cert, srcdir + "/" + "ca-1.der")
    self_sign_csr(db, srcdir, db + "/ca.csr", ca_key, 2, ca_ext, "ca-2")
    os.remove(ca_cert);

generate_certs()
