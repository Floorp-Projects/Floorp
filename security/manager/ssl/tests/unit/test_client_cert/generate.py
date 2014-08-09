#!/usr/bin/python
# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# After running this file you MUST modify ClientAuthServer.cpp to change the
# fingerprint of the client cert

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

# Remove unnecessary .der file
os.remove(dest_dir + "/" + name + ".der")

print ("You now MUST modify ClientAuthServer.cpp to ensure the xpchell debug " +
       "certificate there matches this newly generated one\n")
