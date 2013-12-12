# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file requires openssl 1.0.0 at least

import os
import random
import pexpect

def init_dsa(db_dir):
    """
    Initialize dsa parameters

    Sets up a set of params to be reused for DSA key generation

    Arguments:
      db_dir     -- location of the temporary params for the certificate
    """
    dsa_key_params = db_dir + "/dsa_param.pem"
    os.system("openssl dsaparam -out " + dsa_key_params + " 2048")


def generate_cert_generic(db_dir, dest_dir, serial_num,  key_type, name,
                          ext_text, signer_key_filename = "",
                          signer_cert_filename = "",
                          subject_string = ""):
    """
    Generate an x509 certificate with a sha256 signature

    Preconditions:
      if dsa keys are to be generated init_dsa must have been called before.


    Arguments:
      db_dir     -- location of the temporary params for the certificate
      dest_dir   -- location of the x509 cert
      serial_num -- serial number for the cert (must be unique for each signer
                    key)
      key_type   -- the type of key generated: potential values: 'rsa', 'dsa',
                    or any of the curves found by 'openssl ecparam -list_curves'
      name       -- the common name for the cert, will match the prefix of the
                    output cert
      ext_text   -- the text for the x509 extensions to be added to the
                    certificate
      signer_key_filename -- the filename of the key from which the cert will
                    be signed if null the cert will be self signed (think CA
                    roots).
      signer_cert_filename -- the certificate that will sign the certificate
                    (used to extract signer info) it must be in DER format.

    output:
      key_name   -- the filename of the key file (PEM format)
      cert_name  -- the filename of the output certificate (DER format)
    """
    key_name = db_dir + "/"+ name + ".key"
    if key_type == 'rsa':
      os.system ("openssl genpkey -algorithm RSA -out " + key_name +
                 " -pkeyopt rsa_keygen_bits:2048")
    elif key_type == 'dsa':
      dsa_key_params = db_dir + "/dsa_param.pem"
      os.system("openssl gendsa -out " + key_name + "  " + dsa_key_params)
    else:
      #assume is ec
      os.system("openssl ecparam -out " + key_name + " -name "+ key_type +
                " -genkey");
    csr_name =  db_dir + "/"+ name + ".csr"
    if not subject_string:
      subject_string = '/CN=' + name
    os.system ("openssl req -new -key " + key_name + " -days 3650" +
               " -extensions v3_ca -batch -out " + csr_name +
               " -utf8 -subj '" + subject_string + "'")

    extensions_filename = db_dir + "/openssl-exts"
    f = open(extensions_filename,'w')
    f.write(ext_text)
    f.close()

    cert_name =  dest_dir + "/"+ name + ".der"
    if not signer_key_filename:
        signer_key_filename = key_name;
        os.system ("openssl x509 -req -sha256 -days 3650 -in " + csr_name +
                   " -signkey " + signer_key_filename +
                   " -set_serial " + str(serial_num) +
                   " -extfile " + extensions_filename +
                   " -outform DER -out "+ cert_name)
    else:
        os.system ("openssl x509 -req -sha256 -days 3650 -in " + csr_name +
                   " -CAkey " + signer_key_filename +
                   " -CA " + signer_cert_filename + " -CAform DER " +
                   " -set_serial " + str(serial_num) + " -out " + cert_name +
                   " -outform DER  -extfile " + extensions_filename)
    return key_name, cert_name



def generate_int_and_ee(db_dir, dest_dir, ca_key, ca_cert, name, int_ext_text,
                        ee_ext_text, key_type = 'rsa'):
    """
    Generate an intermediate and ee signed by the generated intermediate. The
    name of the intermediate files will be the name '.der' or '.key'. The name
    of the end entity files with be "ee-"+ name plus the appropiate prefixes.
    The serial number will be generated radomly so it is potentially possible
    to have problem (but very unlikely).

    Arguments:
      db_dir     -- location of the temporary params for the certificate
      dest_dir   -- location of the x509 cert
      ca_key     -- The filename of the key that will be used to sign the
                    intermediate (PEM FORMAT)
      ca_cert    -- The filename of the cert that will be used to sign the
                    intermediate, it MUST be the private key for the ca_key.
                    The file must be in DER format.
      name       -- the common name for the intermediate, will match the prefix
                    of the output intermediate. The ee will have the name
                    prefixed with "ee-"
      int_ext_text  -- the text for the x509 extensions to be added to the
                    intermediate certificate
      ee_ext_text  -- the text for the x509 extensions to be added to the
                    end entity certificate
      key_type   -- the type of key generated: potential values: 'rsa', 'dsa',
                    or any of the curves found by 'openssl ecparam -list_curves'

    output:
      int_key   -- the filename of the intermeidate key file (PEM format)
      int_cert  -- the filename of the intermediate certificate (DER format)
      ee_key    -- the filename of the end entity key file (PEM format)
      ee_cert   -- the filename of the end entity certficate (DER format)

    """
    [int_key, int_cert] = generate_cert_generic(db_dir, dest_dir,
                                                random.randint(100,40000000),
                                                key_type, "int-" + name,
                                                int_ext_text,
                                                ca_key, ca_cert)
    [ee_key, ee_cert] = generate_cert_generic(db_dir, dest_dir,
                                              random.randint(100,40000000),
                                              key_type,  name,
                                              ee_ext_text, int_key, int_cert)

    return int_key, int_cert, ee_key, ee_cert

def generate_pkcs12(db_dir, dest_dir, der_cert_filename, key_pem_filename,
                    prefix):
    """
    Generate a pkcs12 file for a given certificate  name (in der format) and
    a key filename (key in pem format). The output file will have an empty
    password.

    Arguments:
    input:
      db_dir     -- location of the temporary params for the certificate
      dest_dir   -- location of the x509 cert
      der_cert_filename -- the filename of the certificate to be included in the
                           output file (DER format)
      key_pem_filename  -- the filename of the private key of the certificate to
                           (PEM format)
      prefix     -- the name to be prepended to the output pkcs12 file.
    output:
      pk12_filename -- the filename of the outgoing pkcs12 output file
    """
    #make pem cert file  from der filename
    pem_cert_filename = db_dir + "/" + prefix + ".pem"
    pk12_filename = dest_dir + "/" + prefix + ".p12"
    os.system("openssl x509 -inform der -in " + der_cert_filename + " -out " +
                pem_cert_filename )
    #now make pkcs12 file
    child = pexpect.spawn("openssl pkcs12 -export -in " + pem_cert_filename +
                          " -inkey " + key_pem_filename + " -out " +
                          pk12_filename)
    child.expect('Enter Export Password:')
    child.sendline('')
    child.expect('Verifying - Enter Export Password:')
    child.sendline('')
    child.expect(pexpect.EOF)
    return pk12_filename

