#!/usr/bin/python
# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import tempfile, os, sys

libpath = os.path.abspath('../psm_common_py')
sys.path.append(libpath)
import CertUtils

srcdir = os.getcwd()
db = tempfile.mkdtemp()

def generate_child_cert(db_dir, dest_dir, noise_file, name, ca_nick,
                        cert_version, do_bc, is_ee):
   return CertUtils.generate_child_cert(db_dir, dest_dir, noise_file, name,
                                        ca_nick, cert_version, do_bc, is_ee, '')

def generate_ee_family(db_dir, dest_dir, noise_file, ca_name):
  name = "v1_ee-"+ ca_name;
  generate_child_cert(db_dir, dest_dir, noise_file, name, ca_name, 1, False, True)
  name = "v1_bc_ee-"+ ca_name;
  generate_child_cert(db_dir, dest_dir, noise_file, name, ca_name, 1, True, True)

  name = "v2_ee-"+ ca_name;
  generate_child_cert(db_dir, dest_dir, noise_file, name, ca_name, 2, False, True)
  name = "v2_bc_ee-"+ ca_name;
  generate_child_cert(db_dir, dest_dir, noise_file, name, ca_name, 2, True, True)

  name = "v3_missing_bc_ee-"+ ca_name;
  generate_child_cert(db_dir, dest_dir, noise_file, name, ca_name, 3, False, True)
  name = "v3_bc_ee-"+ ca_name;
  generate_child_cert(db_dir, dest_dir, noise_file, name, ca_name, 3, True, True)

  name = "v4_bc_ee-"+ ca_name;
  generate_child_cert(db_dir, dest_dir, noise_file, name, ca_name, 4, True, True)

def generate_intermediates_and_ee_set(db_dir, dest_dir, noise_file, ca_name):
  name =  "v1_int-" + ca_name;
  generate_child_cert(db, srcdir, noise_file, name, ca_name, 1, False, False)
  generate_ee_family(db, srcdir, noise_file, name)
  name = "v1_int_bc-" + ca_name;
  generate_child_cert(db, srcdir, noise_file, name, ca_name, 1, True, False)
  generate_ee_family(db, srcdir, noise_file, name)

  name =  "v2_int-" + ca_name;
  generate_child_cert(db, srcdir, noise_file, name, ca_name, 2, False, False)
  generate_ee_family(db, srcdir, noise_file, name)
  name = "v2_int_bc-" + ca_name;
  generate_child_cert(db, srcdir, noise_file, name, ca_name, 2, True, False)
  generate_ee_family(db, srcdir, noise_file, name)

  name =  "v3_int_missing_bc-" + ca_name;
  generate_child_cert(db, srcdir, noise_file, name, ca_name, 3, False, False)
  generate_ee_family(db, srcdir, noise_file, name)
  name = "v3_int-" + ca_name;
  generate_child_cert(db, srcdir, noise_file, name, ca_name, 3, True, False)
  generate_ee_family(db, srcdir, noise_file, name)

def generate_ca(db_dir, dest_dir, noise_file,  name, version, do_bc):
  CertUtils.generate_ca_cert(db_dir, dest_dir, noise_file,  name, version, do_bc)
  generate_intermediates_and_ee_set(db_dir, dest_dir, noise_file, name)

def generate_certs():
  [noise_file, pwd_file] = CertUtils.init_nss_db(db)
  generate_ca(db, srcdir, noise_file, "v1_ca", 1, False )
  generate_ca(db, srcdir, noise_file, "v1_ca_bc", 1, True)
  generate_ca(db, srcdir, noise_file, "v2_ca", 2, False )
  generate_ca(db, srcdir, noise_file, "v2_ca_bc", 2, True)
  generate_ca(db, srcdir, noise_file, "v3_ca", 3, True )
  generate_ca(db, srcdir, noise_file, "v3_ca_missing_bc", 3, False)

generate_certs();
