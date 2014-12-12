#!/usr/bin/python
# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import tempfile, os, sys, random

libpath = os.path.abspath("../psm_common_py")
sys.path.append(libpath)

import CertUtils

dest_dir = os.getcwd()
db = tempfile.mkdtemp()

serial = random.randint(100, 40000000)
name = "client-cert"
[key, cert] = CertUtils.generate_cert_generic(db, dest_dir, serial, "rsa",
                                              name, "")
CertUtils.generate_pkcs12(db, dest_dir, cert, key, name)

# Print a blank line and the fingerprint of the cert that ClientAuthServer.cpp
# should be modified with.
print
CertUtils.print_cert_info(cert)
print ('You now MUST update the fingerprint in ClientAuthServer.cpp to match ' +
       'the fingerprint printed above.')

# Remove unnecessary .der file
os.remove(dest_dir + "/" + name + ".der")
