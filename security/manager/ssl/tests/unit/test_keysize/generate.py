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

ca_ext_text = ('basicConstraints = critical, CA:TRUE\n' +
               'keyUsage = keyCertSign, cRLSign\n')
ee_ext_text = ''

generated_ev_root_filenames = []
generated_certs = []

def generate_and_maybe_import_cert(key_type, cert_name_prefix, cert_name_suffix,
                                   base_ext_text, signer_key_filename,
                                   signer_cert_filename, key_size, generate_ev):
    """
    Generates a certificate and imports it into the NSS DB if appropriate.
    If an equivalent certificate has already been generated, it is reused.

    Arguments:
      key_type -- the type of key generated: potential values: 'rsa', or any of
                  the curves found by 'openssl ecparam -list_curves'
      cert_name_prefix -- prefix of the generated cert name
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
      key_size -- public key size for RSA certs
      generate_ev -- whether an EV cert should be generated

    Output:
      cert_name -- the resultant (nick)name of the certificate
      key_filename -- the filename of the key file (PEM format)
      cert_filename -- the filename of the certificate (DER format)
    """
    cert_name = cert_name_prefix + '_' + key_type + '_' + key_size

    # If the suffix is not the empty string, add a hyphen for visual separation
    if cert_name_suffix:
        cert_name += '-' + cert_name_suffix

    ev_ext_text = ''
    subject_string = ('/CN=XPCShell Key Size Testing %s %s-bit' %
                      (key_type, key_size))
    if generate_ev:
        cert_name = 'ev_' + cert_name
        ev_ext_text = (CertUtils.aia_prefix + cert_name + CertUtils.aia_suffix +
                       CertUtils.mozilla_testing_ev_policy)
        subject_string += ' (EV)'

    # Use the organization field to store the cert nickname for easier debugging
    subject_string += '/O=' + cert_name

    # Reuse the existing RSA EV root
    if (generate_ev and key_type == 'rsa' and signer_key_filename == ''
            and signer_cert_filename == '' and key_size == '2048'):
        cert_name = 'evroot'
        key_filename = '../test_ev_certs/evroot.key'
        cert_filename = '../test_ev_certs/evroot.der'
        CertUtils.import_cert_and_pkcs12(srcdir, cert_filename,
                                         '../test_ev_certs/evroot.p12',
                                         cert_name, ',,')
        return [cert_name, key_filename, cert_filename]

    # Don't regenerate a previously generated cert
    for cert in generated_certs:
        if cert_name == cert[0]:
            return cert

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
        key_size)
    generated_certs.append([cert_name, key_filename, cert_filename])

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

    return [cert_name, key_filename, cert_filename]

def generate_cert_chain(root_key_type, root_key_size, int_key_type, int_key_size,
                        ee_key_type, ee_key_size, generate_ev):
    """
    Generates a certificate chain and imports the individual certificates into
    the NSS DB if appropriate.

    Arguments:
    (root|int|ee)_key_type -- the type of key generated: potential values: 'rsa',
                              or any of the curves found by
                              'openssl ecparam -list_curves'
    (root|int|ee)_key_size -- public key size for the relevant cert
    generate_ev -- whether EV certs should be generated
    """
    [root_nick, root_key_file, root_cert_file] = generate_and_maybe_import_cert(
        root_key_type,
        'root',
        '',
        ca_ext_text,
        '',
        '',
        root_key_size,
        generate_ev)

    [int_nick, int_key_file, int_cert_file] = generate_and_maybe_import_cert(
        int_key_type,
        'int',
        root_nick,
        ca_ext_text,
        root_key_file,
        root_cert_file,
        int_key_size,
        generate_ev)

    generate_and_maybe_import_cert(
        ee_key_type,
        'ee',
        int_nick,
        ee_ext_text,
        int_key_file,
        int_cert_file,
        ee_key_size,
        generate_ev)

def generate_rsa_chains(inadequate_key_size, adequate_key_size, generate_ev):
    """
    Generates various RSA chains with different combinations of adequately and
    inadequately sized certs.

    Arguments:
      inadequate_key_size -- a string defining the inadequate public key size
                             for the generated certs
      adequate_key_size -- a string defining the adequate public key size for
                           the generated certs
      generate_ev -- whether EV certs should be generated
    """
    # Generate chain with certs that have adequate sizes
    generate_cert_chain('rsa', adequate_key_size,
                        'rsa', adequate_key_size,
                        'rsa', adequate_key_size,
                        generate_ev)

    # Generate chain with a root cert that has an inadequate size
    generate_cert_chain('rsa', inadequate_key_size,
                        'rsa', adequate_key_size,
                        'rsa', adequate_key_size,
                        generate_ev)

    # Generate chain with an intermediate cert that has an inadequate size
    generate_cert_chain('rsa', adequate_key_size,
                        'rsa', inadequate_key_size,
                        'rsa', adequate_key_size,
                        generate_ev)

    # Generate chain with an end entity cert that has an inadequate size
    generate_cert_chain('rsa', adequate_key_size,
                        'rsa', adequate_key_size,
                        'rsa', inadequate_key_size,
                        generate_ev)

def generate_ecc_chains():
    generate_cert_chain('prime256v1', '256',
                        'secp384r1', '384',
                        'secp521r1', '521',
                        False)
    generate_cert_chain('prime256v1', '256',
                        'secp224r1', '224',
                        'prime256v1', '256',
                        False)
    generate_cert_chain('prime256v1', '256',
                        'prime256v1', '256',
                        'secp224r1', '224',
                        False)
    generate_cert_chain('secp224r1', '224',
                        'prime256v1', '256',
                        'prime256v1', '256',
                        False)
    generate_cert_chain('prime256v1', '256',
                        'prime256v1', '256',
                        'secp256k1', '256',
                        False)
    generate_cert_chain('secp256k1', '256',
                        'prime256v1', '256',
                        'prime256v1', '256',
                        False)

def generate_combination_chains():
    generate_cert_chain('rsa', '2048',
                        'prime256v1', '256',
                        'secp384r1', '384',
                        False)
    generate_cert_chain('rsa', '2048',
                        'prime256v1', '256',
                        'secp224r1', '224',
                        False)
    generate_cert_chain('prime256v1', '256',
                        'rsa', '1016',
                        'prime256v1', '256',
                        False)

# Create a NSS DB for use by the OCSP responder.
CertUtils.init_nss_db(srcdir)

generate_rsa_chains('1016', '1024', False)
generate_rsa_chains('2040', '2048', True)
generate_ecc_chains()
generate_combination_chains()

# Print a blank line and the information needed to enable EV for any roots
# generated by this script.
print
for cert_filename in generated_ev_root_filenames:
    CertUtils.print_cert_info(cert_filename)
print ('You now MUST update the compiled test EV root information to match ' +
       'the EV root information printed above.')
