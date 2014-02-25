#!/usr/bin/python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

CA_basic_constraints = "basicConstraints = critical, CA:TRUE\n"
EE_basic_constraints = "basicConstraints = CA:FALSE\n"

CA_full_ku = ("keyUsage = keyCertSign, cRLSign\n")

authority_key_ident = "authorityKeyIdentifier = keyid, issuer\n"
subject_key_ident = "subjectKeyIdentifier = hash\n"

def generate_family(db_dir, dst_dir, ca_key, ca_cert, base_name):
    key_type = 'rsa'
    ee_ext_base = EE_basic_constraints + authority_key_ident;
    #cn =foo.com
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    10,
                                    key_type,
                                    'cn-www.foo.com-'+ base_name,
                                    ee_ext_base,
                                    ca_key,
                                    ca_cert,
                                    '/CN=www.foo.com')
    #cn = foo.org
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    11,
                                    key_type,
                                    'cn-www.foo.org-'+ base_name,
                                    ee_ext_base,
                                    ca_key,
                                    ca_cert,
                                    '/CN=www.foo.org')
    #cn = foo.com, alt= foo.org
    alt_name_ext = 'subjectAltName =DNS:*.foo.org'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    12,
                                    key_type,
                                    'cn-www.foo.com-alt-foo.org-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/CN=www.foo.com')
    #cn = foo.org, alt= foo.com
    alt_name_ext = 'subjectAltName =DNS:*.foo.com'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    13,
                                    key_type,
                                    'cn-www.foo.org-alt-foo.com-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/CN=www.foo.org')
    #cn = foo.com, alt=foo.com
    alt_name_ext = 'subjectAltName =DNS:*.foo.com'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    14,
                                    key_type,
                                    'cn-www.foo.com-alt-foo.com-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/CN=www.foo.com')
    #cn = foo.org, alt=foo.org
    alt_name_ext = 'subjectAltName =DNS:*.foo.org'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    15,
                                    key_type,
                                    'cn-www.foo.org-alt-foo.org-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/CN=www.foo.org')

    #cn = foo.com, alt=foo.com,a.a.us,b.a.us
    alt_name_ext = 'subjectAltName =DNS:*.foo.com,DNS:*.a.a.us,DNS:*.b.a.us'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    16,
                                    key_type,
                                    'cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/CN=www.foo.com')



    #cn =foo.com O=bar C=US
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    17,
                                    key_type,
                                    'cn-www.foo.com_o-bar_c-us-'+ base_name,
                                    ee_ext_base,
                                    ca_key,
                                    ca_cert,
                                    '/C=US/O=bar/CN=www.foo.com')

    #cn = foo.org O=bar C=US
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    18,
                                    key_type,
                                    'cn-www.foo.org_o-bar_c-us-'+ base_name,
                                    ee_ext_base,
                                    ca_key,
                                    ca_cert,
                                    '/C=US/O=bar/CN=www.foo.org')
    #cn = foo.com, alt= foo.org
    alt_name_ext = 'subjectAltName =DNS:*.foo.org'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    19,
                                    key_type,
                                    'cn-www.foo.com_o-bar_c-us-alt-foo.org-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/C=US/O=bar/CN=www.foo.com')
    #cn = foo.org, alt= foo.com
    alt_name_ext = 'subjectAltName =DNS:*.foo.com'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    20,
                                    key_type,
                                    'cn-www.foo.org_o-bar_c-us-alt-foo.com-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/C=US/O=bar/CN=www.foo.org')
    #cn = foo.com, alt=foo.com
    alt_name_ext = 'subjectAltName =DNS:*.foo.com'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    21,
                                    key_type,
                                    'cn-www.foo.com_o-bar_c-us-alt-foo.com-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/C=US/O=bar/CN=www.foo.com')
    #cn = foo.org, alt=foo.org
    alt_name_ext = 'subjectAltName =DNS:*.foo.org'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    22,
                                    key_type,
                                    'cn-www.foo.org_o-bar_c-us-alt-foo.org-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/C=US/O=bar/CN=www.foo.org')

    #cn = foo.com, alt=foo.com,a.a.us.com,b.a.us
    alt_name_ext = 'subjectAltName =DNS:*.foo.com,DNS:*.a.a.us,DNS:*.b.a.us'
    CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    23,
                                    key_type,
                                    'cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-'+ base_name,
                                    ee_ext_base + alt_name_ext,
                                    ca_key,
                                    ca_cert,
                                    '/C=US/O=bar/CN=www.foo.com')




def self_sign_csr(db_dir, dst_dir, csr_name, key_file, serial_num, ext_text,
                  out_prefix):
    extensions_filename = db_dir + "/openssl-exts"
    f = open(extensions_filename, 'w')
    f.write(ext_text)
    f.close()
    cert_name = dst_dir + "/" + out_prefix + ".der"
    os.system ("openssl x509 -req -sha256 -days 3650 -in " + csr_name +
               " -signkey " + key_file +
               " -set_serial " + str(serial_num) +
               " -extfile " + extensions_filename +
               " -outform DER -out " + cert_name)



def generate_certs():
    key_type = 'rsa'
    ca_ext = CA_basic_constraints + CA_full_ku + subject_key_ident;
    ee_ext_text = (EE_basic_constraints + authority_key_ident)
    [ca_key, ca_cert] = CertUtils.generate_cert_generic(db,
                                                        srcdir,
                                                        1,
                                                        key_type,
                                                        'ca-nc',
                                                         ca_ext)
    #now the constrained via perm
    name = 'int-nc-perm-foo.com-ca-nc'
    name_constraints = "nameConstraints = permitted;DNS:foo.com\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    101,
                                    key_type,
                                    name,
                                    ca_ext + authority_key_ident + name_constraints,
                                    ca_key,
                                    ca_cert)
    generate_family(db, srcdir, int_key, int_cert, name)

    #now the constrained via excl
    name = 'int-nc-excl-foo.com-ca-nc'
    name_constraints = "nameConstraints = excluded;DNS:foo.com\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    102,
                                    key_type,
                                    name,
                                    ca_ext + name_constraints + authority_key_ident,
                                    ca_key,
                                    ca_cert)
    generate_family(db, srcdir, int_key, int_cert, name)

    #now constrained to permitted: O=bar C=US
    name = 'int-nc-c-us-ca-nc'
    name_constraints = "nameConstraints = permitted;dirName:dir_sect\n[dir_sect]\nC=US\n\n\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    103,
                                    key_type,
                                    name,
                                    ca_ext + authority_key_ident + name_constraints,
                                    ca_key,
                                    ca_cert)
    generate_family(db, srcdir, int_key, int_cert, name)

    #now make a subCA that is also constrainted to foo.com (combine constraints) 
    name = 'int-nc-foo.com-int-nc-c-us-ca-nc'
    name_constraints = "nameConstraints = permitted;DNS:foo.com\n\n\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    104,
                                    key_type,
                                    name,
                                    ca_ext + name_constraints + authority_key_ident,
                                    int_key,
                                    int_cert,
                                    '/C=US/CN='+ name)
    generate_family(db, srcdir, int_key, int_cert, name)


    #now single intermediate constrainted to  permitted O=bar C=US & DNS foo.com
    name = 'int-nc-perm-foo.com_c-us-ca-nc'
    name_constraints = "nameConstraints = permitted;DNS:foo.com,permitted;dirName:dir_sect\n[dir_sect]\nC=US\n\n\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    105,
                                    key_type,
                                    name,
                                    ca_ext + authority_key_ident + name_constraints,
                                    ca_key,
                                    ca_cert)
    generate_family(db, srcdir, int_key, int_cert, name)

    #now constrainted to permitted C=UK (all ee must fail)
    name = 'int-nc-perm-c-uk-ca-nc'
    name_constraints = "nameConstraints = permitted;dirName:dir_sect\n[dir_sect]\nC=UK\n\n\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    106,
                                    key_type,
                                    name,
                                    ca_ext + authority_key_ident + name_constraints,
                                    ca_key,
                                    ca_cert)
    generate_family(db, srcdir, int_key, int_cert, name)

    #now an unconstrained sub intermediate from the UK cert (all ee must fail) not in the same name space
    name = 'int-c-us-int-nc-perm-c-uk-ca-nc'
    #name_constraints = "nameConstraints = permitted;DNS:foo.com\n\n\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    108,
                                    key_type,
                                    name,
                                    ca_ext + authority_key_ident,
                                    int_key,
                                    int_cert,
                                    '/C=US/CN='+ name)
    generate_family(db, srcdir, int_key, int_cert, name)

    #now we generate permitted to foo.com and example2.com
    name = 'int-nc-foo.com_a.us'
    name_constraints = "nameConstraints = permitted;DNS:foo.com,permitted;DNS:a.us\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    109,
                                    key_type,
                                    name,
                                    ca_ext + authority_key_ident + name_constraints,
                                    ca_key,
                                    ca_cert)
    generate_family(db, srcdir, int_key, int_cert, name)

    #A sub ca contrained to foo.com with signer constrained to foo.com and example2.com
    name = 'int-nc-foo.com-int-nc-foo.com_a.us'
    name_constraints = "nameConstraints = permitted;DNS:foo.com\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    110,
                                    key_type,
                                    name,
                                    ca_ext + authority_key_ident + name_constraints,
                                    ca_key,
                                    ca_cert)
    generate_family(db, srcdir, int_key, int_cert, name)



    #now we generate a root that is name constrained
    name_constraints = "nameConstraints = permitted;DNS:foo.com\n "
    [ca_key, ca_cert] = CertUtils.generate_cert_generic(db,
                                                        srcdir,
                                                        1,
                                                        key_type,
                                                        'ca-nc-perm-foo.com',
                                                        ca_ext + name_constraints)

    #and an unconstrained int
    name = 'int-ca-nc-perm-foo.com'
    name_constraints = "\n"
    [int_key, int_cert] = CertUtils.generate_cert_generic(db,
                                    srcdir,
                                    111,
                                    key_type,
                                    name,
                                    ca_ext + name_constraints + authority_key_ident,
                                    ca_key,
                                    ca_cert)
    generate_family(db, srcdir, int_key, int_cert, name) 


generate_certs()
