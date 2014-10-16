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

ca_ext_text = ('basicConstraints = critical, CA:TRUE\n' +
               'keyUsage = keyCertSign, cRLSign\n')
ee_ext_text = ''

aia_prefix = 'authorityInfoAccess = OCSP;URI:http://www.example.com:8888/'
aia_suffix = '/\n'

mozilla_testing_ev_policy = ('certificatePolicies = @v3_ca_ev_cp\n\n' +
                             '[ v3_ca_ev_cp ]\n' +
                             'policyIdentifier = ' +
                             '1.3.6.1.4.1.13769.666.666.666.1.500.9.1\n\n' +
                             'CPS.1 = "http://mytestdomain.local/cps"')

generated_ev_root_filenames = []

def generate_and_maybe_import_cert(key_type, cert_name_suffix, base_ext_text,
                                   signer_key_filename, signer_cert_filename,
                                   dsa_param_filename, key_size, generate_ev):
    """
    Generates a certificate and imports it into the NSS DB if appropriate.

    Arguments:
      key_type -- the type of key generated: potential values: 'rsa', 'dsa',
                  or any of the curves found by 'openssl ecparam -list_curves'
      cert_name_suffix -- suffix of the generated cert name
      base_ext_text -- the base text for the x509 extensions to be added to the
                       certificate (extra extensions will be added if generating
                       an EV cert)
      signer_key_filename -- the filename of the key from which the cert will
                             be signed. If an empty string is passed in the cert
                             will be self signed (think CA roots).
      signer_cert_filename -- the filename of the signer cert that will sign the
                              certificate being generated. Ignored if an empty
                              string is passed in for signer_key_filename.
                              Must be in DER format.
      dsa_param_filename -- the filename for the DSA param file
      key_size -- public key size for RSA certs
      generate_ev -- whether an EV cert should be generated

    Output:
      key_filename -- the filename of the key file (PEM format)
      cert_filename -- the filename of the certificate (DER format)
    """
    cert_name = key_type + cert_name_suffix
    ev_ext_text = ''
    subject_string = ('/CN=XPCShell Key Size Testing %s %s-bit' %
                      (key_type, key_size))
    if generate_ev:
        cert_name = 'ev-' + cert_name
        ev_ext_text = (aia_prefix + cert_name + aia_suffix +
                       mozilla_testing_ev_policy)
        subject_string += ' (EV)'

    # Use the organization field to store the cert nickname for easier debugging
    subject_string += '/O=' + cert_name

    [key_filename, cert_filename] = CertUtils.generate_cert_generic(
        db_dir,
        srcdir,
        random.randint(100, 40000000),
        key_type,
        cert_name,
        base_ext_text + ev_ext_text,
        signer_key_filename,
        signer_cert_filename,
        subject_string,
        dsa_param_filename,
        key_size)

    if generate_ev:
        # The dest_dir argument of generate_pkcs12() is also set to db_dir as
        # the .p12 files do not need to be kept once they have been imported.
        pkcs12_filename = CertUtils.generate_pkcs12(db_dir, db_dir,
                                                    cert_filename, key_filename,
                                                    cert_name)
        CertUtils.import_cert_and_pkcs12(srcdir, cert_filename, pkcs12_filename,
                                         cert_name, ',,')

        if not signer_key_filename:
            generated_ev_root_filenames.append(cert_filename)

    return [key_filename, cert_filename]

def generate_certs(key_type, bad_key_size, ok_key_size, generate_ev):
    """
    Generates the various certificates used by the key size tests.

    Arguments:
      key_type -- the type of key generated: potential values: 'rsa', 'dsa',
                  or any of the curves found by 'openssl ecparam -list_curves'
      bad_key_size -- the public key size bad certs should have
      ok_key_size -- the public key size OK certs should have
      generate_ev -- whether an EV cert should be generated
    """
    if key_type == 'dsa':
        CertUtils.init_dsa(db_dir, dsaBad_param_filename, bad_key_size)
        CertUtils.init_dsa(db_dir, dsaOK_param_filename, ok_key_size)

    # OK Chain
    if generate_ev and key_type == 'rsa':
        # Reuse the existing RSA EV root
        caOK_cert_name = 'evroot'
        caOK_key = '../test_ev_certs/evroot.key'
        caOK_cert = '../test_ev_certs/evroot.der'
        caOK_pkcs12_filename = '../test_ev_certs/evroot.p12'
        CertUtils.import_cert_and_pkcs12(srcdir, caOK_cert, caOK_pkcs12_filename,
                                         caOK_cert_name, ',,')
    else:
        [caOK_key, caOK_cert] = generate_and_maybe_import_cert(
            key_type,
            '-caOK',
            ca_ext_text,
            '',
            '',
            dsaOK_param_filename,
            ok_key_size,
            generate_ev)

    [intOK_key, intOK_cert] = generate_and_maybe_import_cert(
        key_type,
        '-intOK-caOK',
        ca_ext_text,
        caOK_key,
        caOK_cert,
        dsaOK_param_filename,
        ok_key_size,
        generate_ev)

    generate_and_maybe_import_cert(
        key_type,
        '-eeOK-intOK-caOK',
        ee_ext_text,
        intOK_key,
        intOK_cert,
        dsaOK_param_filename,
        ok_key_size,
        generate_ev)

    # Bad CA
    [caBad_key, caBad_cert] = generate_and_maybe_import_cert(
        key_type,
        '-caBad',
        ca_ext_text,
        '',
        '',
        dsaBad_param_filename,
        bad_key_size,
        generate_ev)

    [int_key, int_cert] = generate_and_maybe_import_cert(
        key_type,
        '-intOK-caBad',
        ca_ext_text,
        caBad_key,
        caBad_cert,
        dsaOK_param_filename,
        ok_key_size,
        generate_ev)

    generate_and_maybe_import_cert(
        key_type,
        '-eeOK-intOK-caBad',
        ee_ext_text,
        int_key,
        int_cert,
        dsaOK_param_filename,
        ok_key_size,
        generate_ev)

    # Bad Intermediate
    [intBad_key, intBad_cert] = generate_and_maybe_import_cert(
        key_type,
        '-intBad-caOK',
        ca_ext_text,
        caOK_key,
        caOK_cert,
        dsaBad_param_filename,
        bad_key_size,
        generate_ev)

    generate_and_maybe_import_cert(
        key_type,
        '-eeOK-intBad-caOK',
        ee_ext_text,
        intBad_key,
        intBad_cert,
        dsaOK_param_filename,
        ok_key_size,
        generate_ev)

    # Bad End Entity
    generate_and_maybe_import_cert(
        key_type,
        '-eeBad-intOK-caOK',
        ee_ext_text,
        intOK_key,
        intOK_cert,
        dsaBad_param_filename,
        bad_key_size,
        generate_ev)

# Create a NSS DB for use by the OCSP responder.
CertUtils.init_nss_db(srcdir)

# TODO(bug 636807): SECKEY_PublicKeyStrengthInBits() rounds up the number of
# bits to the next multiple of 8 - therefore the highest key size less than 1024
# that can be tested is 1016, less than 2048 is 2040 and so on.
generate_certs('rsa', '1016', '1024', False)
generate_certs('rsa', '2040', '2048', True)

generate_certs('dsa', '960', '1024', False)

# Print a blank line and the information needed to enable EV for any roots
# generated by this script.
print
for cert_filename in generated_ev_root_filenames:
    CertUtils.print_cert_info_for_ev(cert_filename)
print ('You now MUST update the compiled test EV root information to match ' +
       'the EV root information printed above.')
