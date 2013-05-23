#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# requires openssl >= 1.0.0

import tempfile, os, sys, random
libpath = os.path.abspath('../psm_common_py')

sys.path.append(libpath)

import CertUtils


srcdir = os.getcwd()
db = tempfile.mkdtemp()

CA_basic_constraints = "basicConstraints = critical, CA:TRUE\n"
CA_limited_basic_constraints = "basicConstraints = critical,CA:TRUE, pathlen:0\n"
EE_basic_constraints = "basicConstraints = CA:FALSE\n"

CA_min_ku = "keyUsage = critical, keyCertSign\n"
CA_bad_ku = "keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment, keyAgreement, cRLSign\n"
EE_full_ku ="keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment, keyAgreement\n"

Server_eku = "extendedKeyUsage = critical, serverAuth, clientAuth\n "

key_type = 'rsa'

def generate_int_and_ee2(ca_key, ca_cert, suffix, int_ext_text, ee_ext_text):
    int_name = "int-" + suffix;
    ee_name  = "ee-int-" + suffix;
    int_serial = random.randint(100, 40000000);
    ee_serial = random.randint(100, 40000000);
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                                        srcdir,
                                                        int_serial,
                                                        key_type,
                                                        int_name,
                                                        int_ext_text,
                                                        ca_key,
                                                        ca_cert);
    [ee_key, ee_cert] = CertUtils.generate_cert_generic(db,
                                                        srcdir,
                                                        ee_serial,
                                                        key_type,
                                                        ee_name,
                                                        ee_ext_text,
                                                        int_key,
                                                        int_cert);
    return [int_key, int_cert, ee_key, ee_cert]

def generate_certs():
    ca_name = "ca"
    [ca_key, ca_cert ] = CertUtils.generate_cert_generic(db,
                                                        srcdir,
                                                        1,
                                                        key_type,
                                                        ca_name,
                                                        CA_basic_constraints)
    ee_ext_text = EE_basic_constraints + EE_full_ku

    #now the intermediates
    generate_int_and_ee2(ca_key, ca_cert, "no-extensions", "", ee_ext_text)
    generate_int_and_ee2(ca_key, ca_cert, "not-a-ca", EE_basic_constraints,
                         ee_ext_text)

    [int_key, int_cert, a, b] = generate_int_and_ee2(ca_key, ca_cert,
                                                     "limited-depth",
                                                     CA_limited_basic_constraints,
                                                     ee_ext_text);
    #and a child using this one
    generate_int_and_ee2(int_key, int_cert, "limited-depth-invalid",
                         CA_basic_constraints, ee_ext_text);

    # now we do it again for valid basic constraints but strange eku/ku at the
    # intermediate layer
    generate_int_and_ee2(ca_key, ca_cert, "valid-ku-no-eku",
                         CA_basic_constraints + CA_min_ku, ee_ext_text);
    generate_int_and_ee2(ca_key, ca_cert, "bad-ku-no-eku",
                         CA_basic_constraints + CA_bad_ku, ee_ext_text);
    generate_int_and_ee2(ca_key, ca_cert, "no-ku-no-eku",
                         CA_basic_constraints, ee_ext_text);

    generate_int_and_ee2(ca_key, ca_cert, "valid-ku-server-eku",
                         CA_basic_constraints +  CA_min_ku + Server_eku,
                         ee_ext_text);
    generate_int_and_ee2(ca_key, ca_cert, "bad-ku-server-eku",
                         CA_basic_constraints +  CA_bad_ku + Server_eku,
                         ee_ext_text);
    generate_int_and_ee2(ca_key, ca_cert, "no-ku-server-eku",
                         CA_basic_constraints  + Server_eku, ee_ext_text);


generate_certs()


