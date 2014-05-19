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
    nick_baseurl = { 'no-path-url': "http://www.example.com:8888",
                     'ftp-url': "ftp://www.example.com:8888/",
                     'no-scheme-url': "www.example.com:8888/",
                     'empty-scheme-url': "://www.example.com:8888/",
                     'no-host-url': "http://:8888/",
                     'hTTp-url': "hTTp://www.example.com:8888/hTTp-url",
                     'https-url': "https://www.example.com:8888/https-url",
                     'bad-scheme': "/www.example.com",
                     'empty-port': "http://www.example.com:/",
                     'unknown-scheme': "ttp://www.example.com",
                     'negative-port': "http://www.example.com:-1",
                     'no-scheme-host-port': "/" }
    for nick, url in nick_baseurl.iteritems():
        generate_child_cert(srcdir, srcdir, noise_file, nick, 'int', True, url)

generate_certs()
