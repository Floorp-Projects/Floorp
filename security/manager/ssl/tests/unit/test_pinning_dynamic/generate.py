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

CA_full_ku = ("keyUsage = keyCertSign, cRLSign\n")

authority_key_ident = "authorityKeyIdentifier = keyid, issuer\n"
subject_key_ident = "subjectKeyIdentifier = hash\n"

def generate_family(db_dir, dst_dir, ca_key, ca_cert, base_name):
    key_type = 'rsa'
    ee_ext_base = EE_basic_constraints + authority_key_ident;
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    10,
                                    key_type,
                                    'cn-a.pinning2.example.com-'+ base_name,
                                    ee_ext_base,
                                    ca_key,
                                    ca_cert,
                                    '/CN=a.pinning2.example.com')

    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    11,
                                    key_type,
                                    'cn-x.a.pinning2.example.com-'+ base_name,
                                    ee_ext_base,
                                    ca_key,
                                    ca_cert,
                                    '/CN=x.a.pinning2.example.com')

    alt_name_ext = 'subjectAltName =DNS:a.pinning2.example.com'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    12,
                                    key_type,
                                    'cn-www.example.com-alt-a.pinning2.example-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/CN=www.example.com')

    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    13,
                                    key_type,
                                    'cn-b.pinning2.example.com-'+ base_name,
                                    ee_ext_base,
                                    ca_key,
                                    ca_cert,
                                    '/CN=b.pinning2.example.com')

    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    14,
                                    key_type,
                                    'cn-x.b.pinning2.example.com-'+ base_name,
                                    ee_ext_base,
                                    ca_key,
                                    ca_cert,
                                    '/CN=x.b.pinning2.example.com')

def generate_certs():
    key_type = 'rsa'
    ca_ext = CA_basic_constraints + CA_full_ku + subject_key_ident
    ee_ext_text = (EE_basic_constraints + authority_key_ident)
    [ca_key, ca_cert] = CertUtils.generate_cert_generic(db,
                                                        srcdir,
                                                        1,
                                                        key_type,
                                                        'badca',
                                                         ca_ext)
    generate_family(db, srcdir, ca_key, ca_cert, 'badca')
    ca_cert = 'pinningroot.der'
    ca_key = 'pinningroot.key'
    generate_family(db, srcdir, ca_key, ca_cert, 'pinningroot')

generate_certs()
