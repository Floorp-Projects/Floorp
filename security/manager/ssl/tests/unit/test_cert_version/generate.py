#!/usr/bin/python
# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import tempfile, os, sys, pexpect
import random
import time

srcdir = os.getcwd()
db = tempfile.mkdtemp()

def init_nss_db(db_dir):
  #create noise file
  noise_file = db_dir + "/noise"
  nf = open(noise_file, 'w')
  nf.write(str(time.time()))
  nf.close()
  #create pwd file
  pwd_file = db_dir + "/pwfile"
  pf = open(pwd_file, 'w')
  pf.write("\n")
  pf.close()
  #create nss db
  os.system("certutil -d " + db_dir + " -N -f "+ pwd_file);
  return [noise_file, pwd_file]

def generate_ca_cert(db_dir, dest_dir, noise_file,  name, version, do_bc):
  out_name = dest_dir + "/" + name + ".der"
  base_exec_string = ("certutil -S -z " + noise_file + " -g 2048 -d "+ db_dir +"/ -n " +
                      name +" -v 120 -s 'CN=" + name +
                      ",O=PSM Testing,L=Mountain View,ST=California,C=US'" +
                      " -t C,C,C -x --certVersion="+ str(int(version)))
  if (do_bc):
    child = pexpect.spawn(base_exec_string + " -2")
    child.logfile = sys.stdout
    child.expect('Is this a CA certificate \[y/N\]?')
    child.sendline('y')
    child.expect('Enter the path length constraint, enter to skip \[<0 for unlimited path\]: >')
    child.sendline('')
    child.expect('Is this a critical extension \[y/N\]?')
    child.sendline('')
    child.expect(pexpect.EOF)
  else:
    os.system(base_exec_string)
  os.system("certutil -d "+ db_dir + "/ -L -n "+ name +" -r > " + out_name)

def generate_child_cert(db_dir, dest_dir, noise_file, name, ca_nick, cert_version, do_bc, is_ee):
  out_name = dest_dir + "/" + name + ".der"
  base_exec_string = ("certutil -S -z " + noise_file + " -g 2048 -d "+ db_dir +"/ -n " +
              name +" -v 120 -m "+ str(random.randint(100,40000000)) +" -s 'CN=" + name +
              ",O=PSM Testing,L=Mountain View,ST=California,C=US'" +
              " -t C,C,C -c "+ ca_nick +" --certVersion="+ str(int(cert_version)))
  if (do_bc):
    child = pexpect.spawn(base_exec_string + " -2")
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
    child.expect(pexpect.EOF)
  else:
    os.system(base_exec_string)
  os.system("certutil -d "+ db_dir + "/ -L -n "+ name +" -r > " + out_name)

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
  generate_ca_cert(db_dir, dest_dir, noise_file,  name, version, do_bc)
  generate_intermediates_and_ee_set(db_dir, dest_dir, noise_file, name)

def generate_certs():
  [noise_file, pwd_file] = init_nss_db(db)
  generate_ca(db, srcdir, noise_file, "v1_ca", 1, False )
  generate_ca(db, srcdir, noise_file, "v1_ca_bc", 1, True)
  generate_ca(db, srcdir, noise_file, "v2_ca", 2, False )
  generate_ca(db, srcdir, noise_file, "v2_ca_bc", 2, True)
  generate_ca(db, srcdir, noise_file, "v3_ca", 3, True )
  generate_ca(db, srcdir, noise_file, "v3_ca_missing_bc", 3, False)

generate_certs();
