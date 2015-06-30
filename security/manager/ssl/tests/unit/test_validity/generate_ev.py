#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import math
import os
import random
import sys
import tempfile

sys.path.append(os.path.abspath('../psm_common_py'))
import CertUtils

src_dir = os.getcwd()
temp_dir = tempfile.mkdtemp()

generated_ev_certs = []
def generate_and_import_cert(cert_name_prefix, cert_name_suffix,
                             base_ext_text, signer_key_filename,
                             signer_cert_filename, validity_in_months):
    """
    Generates a certificate and imports it into the NSS DB.

    Arguments:
        cert_name_prefix - prefix of the generated cert name
        cert_name_suffix - suffix of the generated cert name
        base_ext_text - the base text for the x509 extensions to be
                        added to the certificate (extra extensions will
                        be added if generating an EV cert)
        signer_key_filename - the filename of the key from which the
                              cert will be signed. If an empty string is
                              passed in the cert will be self signed
                              (think CA roots).
        signer_cert_filename - the filename of the signer cert that will
                               sign the certificate being generated.
                               Ignored if an empty string is passed in
                               for signer_key_filename.
                               Must be in DER format.
        validity_in_months - the number of months the cert should be
                             valid for.

    Output:
      cert_name - the resultant (nick)name of the certificate
      key_filename - the filename of the key file (PEM format)
      cert_filename - the filename of the certificate (DER format)
    """
    cert_name = 'ev_%s_%u_months' % (cert_name_prefix, validity_in_months)

    # If the suffix is not the empty string, add a hyphen for visual
    # separation
    if cert_name_suffix:
        cert_name += '-' + cert_name_suffix

    subject_string = '/CN=%s' % cert_name
    ev_ext_text = (CertUtils.aia_prefix + cert_name + CertUtils.aia_suffix +
                   CertUtils.mozilla_testing_ev_policy)

    # Reuse the existing RSA EV root
    if (signer_key_filename == '' and signer_cert_filename == ''):
        cert_name = 'evroot'
        key_filename = '../test_ev_certs/evroot.key'
        cert_filename = '../test_ev_certs/evroot.der'
        CertUtils.import_cert_and_pkcs12(src_dir, cert_filename,
                                         '../test_ev_certs/evroot.p12',
                                         cert_name, ',,')
        return [cert_name, key_filename, cert_filename]

    # Don't regenerate a previously generated cert
    for cert in generated_ev_certs:
        if cert_name == cert[0]:
            return cert

    validity_years = math.floor(validity_in_months / 12)
    validity_months = validity_in_months % 12
    [key_filename, cert_filename] = CertUtils.generate_cert_generic(
        temp_dir,
        src_dir,
        random.randint(100, 40000000),
        'rsa',
        cert_name,
        base_ext_text + ev_ext_text,
        signer_key_filename,
        signer_cert_filename,
        subject_string,
        validity_in_days = validity_years * 365 + validity_months * 31)
    generated_ev_certs.append([cert_name, key_filename, cert_filename])

    # The dest_dir argument of generate_pkcs12() is also set to temp_dir
    # as the .p12 files do not need to be kept once they have been
    # imported.
    pkcs12_filename = CertUtils.generate_pkcs12(temp_dir, temp_dir,
                                                cert_filename,
                                                key_filename,
                                                cert_name)
    CertUtils.import_cert_and_pkcs12(src_dir, cert_filename,
                                     pkcs12_filename, cert_name, ',,')

    return [cert_name, key_filename, cert_filename]

def generate_chain(ee_validity_months):
    """
    Generates a certificate chain and imports the individual
    certificates into the NSS DB.
    """
    ca_ext_text = ('basicConstraints = critical, CA:TRUE\n' +
                   'keyUsage = keyCertSign, cRLSign\n')

    [root_nick, root_key_file, root_cert_file] = generate_and_import_cert(
        'root',
        '',
        ca_ext_text,
        '',
        '',
        60)

    [int_nick, int_key_file, int_cert_file] = generate_and_import_cert(
        'int',
        root_nick,
        ca_ext_text,
        root_key_file,
        root_cert_file,
        60)

    generate_and_import_cert(
        'ee',
        int_nick,
        '',
        int_key_file,
        int_cert_file,
        ee_validity_months)

# Create a NSS DB for use by the OCSP responder.
[noise_file, pwd_file] = CertUtils.init_nss_db(src_dir)

generate_chain(39)
generate_chain(40)

# Remove unnecessary files
os.remove(noise_file)
os.remove(pwd_file)
