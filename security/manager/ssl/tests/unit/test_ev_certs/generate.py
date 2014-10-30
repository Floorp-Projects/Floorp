#!/usr/bin/python

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

CA_extensions  = ("basicConstraints = critical, CA:TRUE\n"
                  "keyUsage = keyCertSign, cRLSign\n")

aia_prefix = "authorityInfoAccess = OCSP;URI:http://www.example.com:8888/"
aia_suffix ="/\n"
intermediate_crl = ("crlDistributionPoints = " +
                    "URI:http://crl.example.com:8888/root-ev.crl\n")
endentity_crl = ("crlDistributionPoints = " +
                 "URI:http://crl.example.com:8888/ee-crl.crl\n")

mozilla_testing_ev_policy = ("certificatePolicies = @v3_ca_ev_cp\n\n" +
                             "[ v3_ca_ev_cp ]\n" +
                             "policyIdentifier = " +
                               "1.3.6.1.4.1.13769.666.666.666.1.500.9.1\n\n" +
                             "CPS.1 = \"http://mytestdomain.local/cps\"")

anypolicy_policy = ("certificatePolicies = @v3_ca_ev_cp\n\n" +
                    "[ v3_ca_ev_cp ]\n" +
                    "policyIdentifier = " +
                    "2.5.29.32.0\n\n" +
                    "CPS.1 = \"http://mytestdomain.local/cps\"")


def import_untrusted_cert(certfile, nickname):
    os.system("certutil -A -d . -n " + nickname + " -i " + certfile +
              " -t ',,'")

def import_cert_and_pkcs12(certfile, pkcs12file, nickname, trustflags):
    os.system(" certutil -A -d . -n " + nickname + " -i " + certfile + " -t '" +
              trustflags + "'")
    child = pexpect.spawn("pk12util -i " + pkcs12file + "  -d .")
    child.expect('Enter password for PKCS12 file:')
    child.sendline('')
    child.expect(pexpect.EOF)

def init_nss_db():
    nss_db_files = [ "cert8.db", "key3.db", "secmod.db" ]
    for file in nss_db_files:
        if os.path.isfile(file):
            os.remove(file)
    #now create DB
    child = pexpect.spawn("certutil -N -d .")
    child.expect("Enter new password:")
    child.sendline('')
    child.expect('Re-enter password:')
    child.sendline('')
    child.expect(pexpect.EOF)
    import_cert_and_pkcs12("evroot.der", "evroot.p12", "evroot", "C,C,C")


def generate_certs():
    init_nss_db()
    ca_cert = 'evroot.der'
    ca_key = 'evroot.key'
    prefix = "ev-valid"
    key_type = 'rsa'
    ee_ext_text = (aia_prefix + prefix + aia_suffix +
                   endentity_crl + mozilla_testing_ev_policy)
    int_ext_text = (CA_extensions + aia_prefix + "int-" + prefix + aia_suffix +
                    intermediate_crl + mozilla_testing_ev_policy)
    [int_key, int_cert, ee_key, ee_cert] = CertUtils.generate_int_and_ee(db,
                                             srcdir,
                                             ca_key,
                                             ca_cert,
                                             prefix,
                                             int_ext_text,
                                             ee_ext_text,
                                             key_type)
    pk12file = CertUtils.generate_pkcs12(db, srcdir, int_cert, int_key,
                                         "int-" + prefix)
    import_cert_and_pkcs12(int_cert, pk12file, "int-" + prefix, ",,")
    import_untrusted_cert(ee_cert, prefix)

    # now we generate an end entity cert with an AIA with no OCSP URL
    no_ocsp_url_ext_aia = ("authorityInfoAccess =" +
                           "caIssuers;URI:http://www.example.com/ca.html\n");
    [no_ocsp_key, no_ocsp_cert] =  CertUtils.generate_cert_generic(db,
                                      srcdir,
                                      random.randint(100, 40000000),
                                      key_type,
                                      'no-ocsp-url-cert',
                                      no_ocsp_url_ext_aia + endentity_crl +
                                      mozilla_testing_ev_policy,
                                      int_key, int_cert);
    import_untrusted_cert(no_ocsp_cert, 'no-ocsp-url-cert');

    # add an ev cert whose intermediate has a anypolicy oid
    prefix = "ev-valid-anypolicy-int"
    ee_ext_text = (aia_prefix + prefix + aia_suffix +
                   endentity_crl + mozilla_testing_ev_policy)
    int_ext_text = (CA_extensions + aia_prefix + "int-" + prefix + aia_suffix +
                    intermediate_crl + anypolicy_policy)

    [int_key, int_cert, ee_key, ee_cert] = CertUtils.generate_int_and_ee(db,
                                             srcdir,
                                             ca_key,
                                             ca_cert,
                                             prefix,
                                             int_ext_text,
                                             ee_ext_text,
                                             key_type)
    pk12file = CertUtils.generate_pkcs12(db, srcdir, int_cert, int_key,
                                         "int-" + prefix)
    import_cert_and_pkcs12(int_cert, pk12file, "int-" + prefix, ",,")
    import_untrusted_cert(ee_cert, prefix)


    [bad_ca_key, bad_ca_cert] = CertUtils.generate_cert_generic( db,
                                      srcdir,
                                      1,
                                      'rsa',
                                      'non-evroot-ca',
                                      CA_extensions)
    pk12file =  CertUtils.generate_pkcs12(db, srcdir, bad_ca_cert, bad_ca_key,
                                          "non-evroot-ca")
    import_cert_and_pkcs12(bad_ca_cert, pk12file, "non-evroot-ca", "C,C,C")
    prefix = "non-ev-root"
    ee_ext_text = (aia_prefix + prefix  + aia_suffix +
                   endentity_crl + mozilla_testing_ev_policy)
    int_ext_text = (CA_extensions + aia_prefix + "int-" + prefix + aia_suffix +
                    intermediate_crl + mozilla_testing_ev_policy)
    [int_key, int_cert, ee_key, ee_cert] = CertUtils.generate_int_and_ee(db,
                                      srcdir,
                                      bad_ca_key,
                                      bad_ca_cert,
                                      prefix,
                                      int_ext_text,
                                      ee_ext_text,
                                      key_type)
    pk12file =  CertUtils.generate_pkcs12(db, srcdir, int_cert, int_key,
                                          "int-" + prefix)
    import_cert_and_pkcs12(int_cert, pk12file, "int-" + prefix, ",,")
    import_untrusted_cert(ee_cert, prefix)



generate_certs()
