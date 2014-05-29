#!/usr/bin/python

import tempfile, os, sys

libpath = os.path.abspath('../psm_common_py')
sys.path.append(libpath)
import CertUtils

srcdir = os.getcwd()
db = tempfile.mkdtemp()

def generate_ca_cert(db_dir, dest_dir, noise_file, name):
    return CertUtils.generate_ca_cert(db_dir, dest_dir, noise_file, name,
                                      3, True)

def generate_child_cert(db_dir, dest_dir, noise_file, name, ca_nick, is_ee,
                        ocsp_url):
    return CertUtils.generate_child_cert(db_dir, dest_dir, noise_file, name,
                                         ca_nick, 3, True, is_ee, ocsp_url)

def generate_certs():
    [noise_file, pwd_file] = CertUtils.init_nss_db(srcdir)
    generate_ca_cert(srcdir, srcdir, noise_file, 'ca')
    generate_child_cert(srcdir, srcdir, noise_file, 'int', 'ca', False, '')
    ocsp_url = "http://www.example.com:8080/"
    generate_child_cert(srcdir, srcdir, noise_file, "a", 'int', True, ocsp_url)
    generate_child_cert(srcdir, srcdir, noise_file, "b", 'int', True, ocsp_url)

generate_certs()
