#!/usr/bin/python
# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
#
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

    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    103,
                                    key_type,
                                    'int',
                                    ca_ext,
                                    ca_key,
                                    ca_cert)

    #now the ee
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    100,
                                    key_type,
                                    'ee',
                                    ee_ext_text,
                                    int_key,
                                    int_cert)





generate_certs()
