#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import tempfile, os, sys
import random

libpath = os.path.abspath('../psm_common_py')

sys.path.append(libpath)

import CertUtils

srcdir = os.getcwd()
db_dir = tempfile.mkdtemp()
dsaBad_param_filename = 'dsaBad_param.pem'
dsaOK_param_filename = 'dsaOK_param.pem'

ca_ext_text = 'basicConstraints = critical, CA:TRUE\n'
ee_ext_text = ''

def generate_certs(key_type, bad_key_size, ok_key_size):
    if key_type == 'dsa':
        CertUtils.init_dsa(db_dir, dsaBad_param_filename, bad_key_size)
        CertUtils.init_dsa(db_dir, dsaOK_param_filename, ok_key_size)

    # OK Chain
    [caOK_key, caOK_cert] = CertUtils.generate_cert_generic(
                                db_dir,
                                srcdir,
                                random.randint(100, 40000000),
                                key_type,
                                key_type + '-caOK',
                                ca_ext_text,
                                dsa_param_filename = dsaOK_param_filename,
                                key_size = ok_key_size)

    [intOK_key, intOK_cert] = CertUtils.generate_cert_generic(
                                  db_dir,
                                  srcdir,
                                  random.randint(100, 40000000),
                                  key_type,
                                  key_type + '-intOK-caOK',
                                  ca_ext_text,
                                  caOK_key,
                                  caOK_cert,
                                  dsa_param_filename = dsaOK_param_filename,
                                  key_size = ok_key_size)

    CertUtils.generate_cert_generic(db_dir,
                                    srcdir,
                                    random.randint(100, 40000000),
                                    key_type,
                                    key_type + '-eeOK-intOK-caOK',
                                    ee_ext_text,
                                    intOK_key,
                                    intOK_cert,
                                    dsa_param_filename = dsaOK_param_filename,
                                    key_size = ok_key_size)

    # Bad CA
    [caBad_key, caBad_cert] = CertUtils.generate_cert_generic(
                                  db_dir,
                                  srcdir,
                                  random.randint(100, 40000000),
                                  key_type,
                                  key_type + '-caBad',
                                  ca_ext_text,
                                  dsa_param_filename = dsaBad_param_filename,
                                  key_size = bad_key_size)

    [int_key, int_cert] = CertUtils.generate_cert_generic(
                              db_dir,
                              srcdir,
                              random.randint(100, 40000000),
                              key_type,
                              key_type + '-intOK-caBad',
                              ca_ext_text,
                              caBad_key,
                              caBad_cert,
                              dsa_param_filename = dsaOK_param_filename,
                              key_size = ok_key_size)

    CertUtils.generate_cert_generic(db_dir,
                                    srcdir,
                                    random.randint(100, 40000000),
                                    key_type,
                                    key_type + '-eeOK-intOK-caBad',
                                    ee_ext_text,
                                    int_key,
                                    int_cert,
                                    dsa_param_filename = dsaOK_param_filename,
                                    key_size = ok_key_size)

    # Bad Intermediate
    [intBad_key, intBad_cert] = CertUtils.generate_cert_generic(
                                    db_dir,
                                    srcdir,
                                    random.randint(100, 40000000),
                                    key_type,
                                    key_type + '-intBad-caOK',
                                    ca_ext_text,
                                    caOK_key,
                                    caOK_cert,
                                    dsa_param_filename = dsaBad_param_filename,
                                    key_size = bad_key_size)

    CertUtils.generate_cert_generic(db_dir,
                                    srcdir,
                                    random.randint(100, 40000000),
                                    key_type,
                                    key_type + '-eeOK-intBad-caOK',
                                    ee_ext_text,
                                    intBad_key,
                                    intBad_cert,
                                    dsa_param_filename = dsaOK_param_filename,
                                    key_size = ok_key_size)

    # Bad End Entity
    CertUtils.generate_cert_generic(db_dir,
                                    srcdir,
                                    random.randint(100, 40000000),
                                    key_type,
                                    key_type + '-eeBad-intOK-caOK',
                                    ee_ext_text,
                                    intOK_key,
                                    intOK_cert,
                                    dsa_param_filename = dsaBad_param_filename,
                                    key_size = bad_key_size)

# SECKEY_PublicKeyStrengthInBits() rounds up the number of bits to the next
# multiple of 8 - therefore the highest key size less than 1024 that can be
# tested at the moment is 1016
generate_certs('rsa', '1016', '1024')

generate_certs('dsa', '960', '1024')
