# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file requires openssl 1.0.0 at least

import os
import random
import pexpect
import time
import sys

aia_prefix = 'authorityInfoAccess = OCSP;URI:http://www.example.com:8888/'
aia_suffix = '/\n'

mozilla_testing_ev_policy = ('certificatePolicies = @v3_ca_ev_cp\n\n' +
                             '[ v3_ca_ev_cp ]\n' +
                             'policyIdentifier = ' +
                             '1.3.6.1.4.1.13769.666.666.666.1.500.9.1\n\n' +
                             'CPS.1 = "http://mytestdomain.local/cps"')

def generate_cert_generic(db_dir, dest_dir, serial_num,  key_type, name,
                          ext_text, signer_key_filename = "",
                          signer_cert_filename = "",
                          subject_string = "",
                          key_size = '2048'):
    """
    Generate an x509 certificate with a sha256 signature

    Arguments:
      db_dir     -- location of the temporary params for the certificate
      dest_dir   -- location of the x509 cert
      serial_num -- serial number for the cert (must be unique for each signer
                    key)
      key_type   -- the type of key generated: potential values: 'rsa' or any
	                of the curves found by 'openssl ecparam -list_curves'
      name       -- the common name for the cert, will match the prefix of the
                    output cert
      ext_text   -- the text for the x509 extensions to be added to the
                    certificate
      signer_key_filename -- the filename of the key from which the cert will
                    be signed if null the cert will be self signed (think CA
                    roots).
      signer_cert_filename -- the certificate that will sign the certificate
                    (used to extract signer info) it must be in DER format.
      key_size   -- public key size for RSA certs

    output:
      key_name   -- the filename of the key file (PEM format)
      cert_name  -- the filename of the output certificate (DER format)
    """
    key_name = db_dir + "/"+ name + ".key"
    if key_type == 'rsa':
      os.system ("openssl genpkey -algorithm RSA -out " + key_name +
                 " -pkeyopt rsa_keygen_bits:" + key_size)
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
      key_type   -- the type of key generated: potential values: 'rsa' or any
	                of the curves found by 'openssl ecparam -list_curves'

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

def import_cert_and_pkcs12(db_dir, cert_filename, pkcs12_filename, nickname,
                           trust_flags):
    """
    Imports a given certificate file and PKCS12 file into the SQL NSS DB.

    Arguments:
      db_dir -- the location of the database and password file
      cert_filename -- the filename of the cert in DER format
      pkcs12_filename -- the filename of the private key of the cert in PEM
                         format
      nickname -- the nickname to assign to the cert
      trust_flags -- the trust flags the cert should have
    """
    os.system('certutil -A -d sql:' + db_dir + ' -n ' + nickname + ' -i ' +
              cert_filename + ' -t "' + trust_flags + '"')
    os.system('pk12util -i ' + pkcs12_filename + ' -d sql:' + db_dir +
              ' -w ' + db_dir + '/pwfile')

def print_cert_info(cert_filename):
    """
    Prints out information (such as fingerprints) for the given cert.
    The information printed is sufficient for enabling EV for the given cert
    if necessary.

    Note: The utility 'pp' is available as part of NSS.

    Arguments:
      cert_filename -- the filename of the cert in DER format
    """
    os.system('pp -t certificate-identity -i ' + cert_filename)

def init_nss_db(db_dir):
    """
    Remove the current nss database in the specified directory and create a new
    nss database with the sql format.
    Arguments
      db_dir -- the desired location of the new database
    output
     noise_file -- the path to a noise file suitable to generate TEST
                   certificates. This does not have enough entropy for a real
                   secret
     pwd_file   -- the patch to the secret file used for the database.
                   this file should be empty.
    """
    nss_db_files = ["cert9.db", "key4.db", "pkcs11.txt"]
    for file in nss_db_files:
        if os.path.isfile(file):
            os.remove(file)
    # create noise file
    noise_file = db_dir + "/noise"
    nf = open(noise_file, 'w')
    nf.write(str(time.time()))
    nf.close()
    # create pwd file
    pwd_file = db_dir + "/pwfile"
    pf = open(pwd_file, 'w')
    pf.write("\n")
    pf.close()
    # create nss db
    os.system("certutil -d sql:" + db_dir + " -N -f " + pwd_file);
    return [noise_file, pwd_file]

def generate_self_signed_cert(db_dir, dest_dir, noise_file, name, version, do_bc, is_ca):
    """
    Creates a new self-signed certificate in an sql NSS database and as a der file
    Arguments:
      db_dir     -- the location of the nss database (in sql format)
      dest_dir   -- the location of for the output file
      noise_file -- the location of a noise file.
      name       -- the nickname of the new certificate in the database and the
                    common name of the certificate
      version    -- the version number of the certificate (valid certs must use
                    3)
      do_bc      -- if the certificate should include the basic constraints
                    (valid ca's should be true)
      is_ca      -- mark the extenstion true or false
    output:
      outname    -- the location of the der file.
    """
    out_name = dest_dir + "/" + name + ".der"
    base_exec_string = ("certutil -S -z " + noise_file + " -g 2048 -d sql:" +
                        db_dir + "/ -n " + name + " -v 120 -s 'CN=" + name +
                        ",O=PSM Testing,L=Mountain View,ST=California,C=US'" +
                        " -t C,C,C -x --certVersion=" + str(int(version)))
    if (do_bc):
        child = pexpect.spawn(base_exec_string + " -2")
        child.logfile = sys.stdout
        child.expect('Is this a CA certificate \[y/N\]?')
        if (is_ca):
          child.sendline('y')
        else:
          child.sendline('N')
        child.expect('Enter the path length constraint, enter to skip \[<0 for unlimited path\]: >')
        child.sendline('')
        child.expect('Is this a critical extension \[y/N\]?')
        child.sendline('')
        child.expect(pexpect.EOF)
    else:
        os.system(base_exec_string)
    os.system("certutil -d sql:" + db_dir + "/ -L -n " + name + " -r > " +
              out_name)
    return out_name

def generate_ca_cert(db_dir, dest_dir, noise_file, name, version, do_bc):
    """
    Creates a new CA certificate in an sql NSS database and as a der file
    Arguments:
      db_dir     -- the location of the nss database (in sql format)
      dest_dir   -- the location of for the output file
      noise_file -- the location of a noise file.
      name       -- the nickname of the new certificate in the database and the
                    common name of the certificate
      version    -- the version number of the certificate (valid certs must use
                    3)
      do_bc      -- if the certificate should include the basic constraints
                    (valid ca's should be true)
    output:
      outname    -- the location of the der file.
    """
    return generate_self_signed_cert(db_dir, dest_dir, noise_file, name, version, do_bc, True)


def generate_child_cert(db_dir, dest_dir, noise_file, name, ca_nick, version,
                        do_bc, is_ee, ocsp_url):
    """
    Creates a new child certificate in an sql NSS database and as a der file
    Arguments:
      db_dir     -- the location of the nss database (in sql format)
      dest_dir   -- the location of for the output file
      noise_file -- the location of a noise file.
      name       -- the nickname of the new certificate in the database and the
                    common name of the certificate
      ca_nick    -- the nickname of the isser of the new certificate
      version    -- the version number of the certificate (valid certs must use
                    3)
      do_bc      -- if the certificate should include the basic constraints
                    (valid ca's should be true)
      is_ee      -- is this and End Entity cert? false means intermediate
      ocsp_url   -- optional location of the ocsp responder for this certificate
                    this is included only if do_bc is set to True
    output:
      outname    -- the location of the der file.
    """

    out_name = dest_dir + "/" + name + ".der"
    base_exec_string = ("certutil -S -z " + noise_file + " -g 2048 -d sql:" +
                        db_dir + "/ -n " + name + " -v 120 -m " +
                        str(random.randint(100, 40000000)) + " -s 'CN=" + name +
                        ",O=PSM Testing,L=Mountain View,ST=California,C=US'" +
                        " -t C,C,C -c " + ca_nick + " --certVersion=" +
                        str(int(version)))
    if (do_bc):
        extra_arguments = " -2"
        if (ocsp_url):
            extra_arguments += " --extAIA"
        child = pexpect.spawn(base_exec_string + extra_arguments)
        child.logfile = sys.stdout
        child.expect('Is this a CA certificate \[y/N\]?')
        if (is_ee):
            child.sendline('N')
        else:
            child.sendline('y')
        child.expect('Enter the path length constraint, enter to skip \[<0 for unlimited path\]: >')
        child.sendline('')
        child.expect('Is this a critical extension \[y/N\]?')
        child.sendline('')
        if (ocsp_url):
            child.expect('\s+Choice >')
            child.sendline('2')
            child.expect('\s+Choice: >')
            child.sendline('7')
            child.expect('Enter data:')
            child.sendline(ocsp_url)
            child.expect('\s+Choice: >')
            child.sendline('0')
            child.expect('Add another location to the Authority Information Access extension \[y/N\]')
            child.sendline('')
            child.expect('Is this a critical extension \[y/N\]?')
            child.sendline('')
        child.expect(pexpect.EOF)
    else:
        os.system(base_exec_string)
    os.system("certutil -d sql:" + db_dir + "/ -L -n " + name + " -r > " +
              out_name)
    return out_name

