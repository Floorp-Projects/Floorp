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
db = tempfile.mkdtemp()

CA_basic_constraints = "basicConstraints=critical,CA:TRUE\n"
CA_limited_basic_constraints = "basicConstraints=critical,CA:TRUE, pathlen:0\n"
EE_basic_constraints = "basicConstraints=CA:FALSE\n"

CA_min_ku = "keyUsage=critical, keyCertSign\n"
CA_bad_ku = ("keyUsage=digitalSignature, nonRepudiation, keyEncipherment," +
                     " dataEncipherment, keyAgreement, cRLSign\n")
EE_full_ku = ("keyUsage=digitalSignature, nonRepudiation, keyEncipherment," +
                      " dataEncipherment, keyAgreement, keyCertSign, cRLSign\n")

Server_eku= "extendedKeyUsage=critical,serverAuth,clientAuth\n"

pk_name = {'rsa': 'rsa', 'dsa': 'dsa', 'p384': 'secp384r1'}


def tamper_cert(cert_name):
    f = open(cert_name, 'r+b')
    f.seek(-3, 2) # third byte from the end to ensure we only touch the
    # signature value. The location for the perturbation ensures that we are
    # modifying just the tbsCertificate without the need of parsing the
    # certificate. Also this guarantees that if a failure occurs it is because
    # of an invalid signature and not another field that might have become
    # invalid.
    b = bytearray(f.read(1))
    for i in range(len(b)):
        b[i] ^= 0x77
    f.seek(-1, 1)
    f.write(b)
    f.close()
    return 1

def generate_certs():

    CertUtils.init_dsa(db)
    ee_ext_text = EE_basic_constraints + EE_full_ku
    for name, key_type in pk_name.iteritems():
        ca_name = "ca-" + name
        [ca_key, ca_cert] = CertUtils.generate_cert_generic(db,
                                                            srcdir,
                                                            random.randint(100,4000000),
                                                            key_type,
                                                            ca_name,
                                                            CA_basic_constraints + CA_min_ku)

        [valid_int_key, valid_int_cert, ee_key, ee_cert] =  (
            CertUtils.generate_int_and_ee(db,
                                          srcdir,
                                          ca_key,
                                          ca_cert,
                                          name + "-valid",
                                          CA_basic_constraints,
                                          ee_ext_text,
                                          key_type) )

        [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                                            srcdir,
                                                            random.randint(100,4000000),
                                                            key_type,
                                                            "int-" + name + "-tampered",
                                                            ee_ext_text,
                                                            ca_key,
                                                            ca_cert)


        [ee_key, ee_cert] = CertUtils.generate_cert_generic(db,
                                                            srcdir,
                                                            random.randint(100,4000000),
                                                            key_type,
                                                            name + "-tampered-int-valid-ee",
                                                            ee_ext_text,
                                                            int_key,
                                                            int_cert)
        #only tamper after ee has been generated
        tamper_cert(int_cert);

        [ee_key, ee_cert] = CertUtils.generate_cert_generic(db,
                                                            srcdir,
                                                            random.randint(100,4000000),
                                                            key_type,
                                                            name + "-valid-int-tampered-ee",
                                                            ee_ext_text,
                                                            valid_int_key,
                                                            valid_int_cert)
        tamper_cert(ee_cert);


generate_certs()
