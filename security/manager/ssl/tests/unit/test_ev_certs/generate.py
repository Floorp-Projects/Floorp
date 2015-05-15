#!/usr/bin/python

import tempfile, os, sys
import random

libpath = os.path.abspath('../psm_common_py')

sys.path.append(libpath)

import CertUtils

srcdir = os.getcwd()
db = tempfile.mkdtemp()

CA_extensions  = ("basicConstraints = critical, CA:TRUE\n"
                  "keyUsage = keyCertSign, cRLSign\n")

intermediate_crl = ("crlDistributionPoints = " +
                    "URI:http://crl.example.com:8888/root-ev.crl\n")
endentity_crl = ("crlDistributionPoints = " +
                 "URI:http://crl.example.com:8888/ee-crl.crl\n")

anypolicy_policy = ("certificatePolicies = @v3_ca_ev_cp\n\n" +
                    "[ v3_ca_ev_cp ]\n" +
                    "policyIdentifier = " +
                    "2.5.29.32.0\n\n" +
                    "CPS.1 = \"http://mytestdomain.local/cps\"")


def import_untrusted_cert(certfile, nickname):
    os.system('certutil -A -d sql:%s -n %s -i %s -t ",,"' %
              (srcdir, nickname, certfile))

def generate_certs():
    ca_cert = 'evroot.der'
    ca_key = 'evroot.key'
    prefix = "ev-valid"
    key_type = 'rsa'
    ee_ext_text = (CertUtils.aia_prefix + prefix + CertUtils.aia_suffix +
                   endentity_crl + CertUtils.mozilla_testing_ev_policy)
    int_ext_text = (CA_extensions + CertUtils.aia_prefix + "int-" + prefix +
                    CertUtils.aia_suffix + intermediate_crl +
                    CertUtils.mozilla_testing_ev_policy)

    CertUtils.init_nss_db(srcdir)
    CertUtils.import_cert_and_pkcs12(srcdir, ca_cert, 'evroot.p12', 'evroot',
                                     'C,C,C')

    [int_key, int_cert, ee_key, ee_cert] = CertUtils.generate_int_and_ee(db,
                                             srcdir,
                                             ca_key,
                                             ca_cert,
                                             prefix,
                                             int_ext_text,
                                             ee_ext_text,
                                             key_type)
    pk12file = CertUtils.generate_pkcs12(db, db, int_cert, int_key,
                                         "int-" + prefix)
    CertUtils.import_cert_and_pkcs12(srcdir, int_cert, pk12file,
                                     'int-' + prefix, ',,')
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
                                      CertUtils.mozilla_testing_ev_policy,
                                      int_key, int_cert);
    import_untrusted_cert(no_ocsp_cert, 'no-ocsp-url-cert');

    # add an ev cert whose intermediate has a anypolicy oid
    prefix = "ev-valid-anypolicy-int"
    ee_ext_text = (CertUtils.aia_prefix + prefix + CertUtils.aia_suffix +
                   endentity_crl + CertUtils.mozilla_testing_ev_policy)
    int_ext_text = (CA_extensions + CertUtils.aia_prefix + "int-" + prefix +
                    CertUtils.aia_suffix + intermediate_crl + anypolicy_policy)

    [int_key, int_cert, ee_key, ee_cert] = CertUtils.generate_int_and_ee(db,
                                             srcdir,
                                             ca_key,
                                             ca_cert,
                                             prefix,
                                             int_ext_text,
                                             ee_ext_text,
                                             key_type)
    pk12file = CertUtils.generate_pkcs12(db, db, int_cert, int_key,
                                         "int-" + prefix)
    CertUtils.import_cert_and_pkcs12(srcdir, int_cert, pk12file,
                                     'int-' + prefix, ',,')
    import_untrusted_cert(ee_cert, prefix)


    [bad_ca_key, bad_ca_cert] = CertUtils.generate_cert_generic( db,
                                      srcdir,
                                      1,
                                      'rsa',
                                      'non-evroot-ca',
                                      CA_extensions)
    pk12file =  CertUtils.generate_pkcs12(db, db, bad_ca_cert, bad_ca_key,
                                          "non-evroot-ca")
    CertUtils.import_cert_and_pkcs12(srcdir, bad_ca_cert, pk12file,
                                     'non-evroot-ca', 'C,C,C')
    prefix = "non-ev-root"
    ee_ext_text = (CertUtils.aia_prefix + prefix  + CertUtils.aia_suffix +
                   endentity_crl + CertUtils.mozilla_testing_ev_policy)
    int_ext_text = (CA_extensions + CertUtils.aia_prefix + "int-" + prefix +
                    CertUtils.aia_suffix + intermediate_crl +
                    CertUtils.mozilla_testing_ev_policy)
    [int_key, int_cert, ee_key, ee_cert] = CertUtils.generate_int_and_ee(db,
                                      srcdir,
                                      bad_ca_key,
                                      bad_ca_cert,
                                      prefix,
                                      int_ext_text,
                                      ee_ext_text,
                                      key_type)
    pk12file =  CertUtils.generate_pkcs12(db, db, int_cert, int_key,
                                          "int-" + prefix)
    CertUtils.import_cert_and_pkcs12(srcdir, int_cert, pk12file,
                                     'int-' + prefix, ',,')
    import_untrusted_cert(ee_cert, prefix)

generate_certs()
