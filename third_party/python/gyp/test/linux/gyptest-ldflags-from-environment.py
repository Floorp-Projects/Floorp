#!/usr/bin/env python

# Copyright (c) 2017 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies the use of linker flags in environment variables.

In this test, gyp and build both run in same local environment.
"""

import TestGyp

import re
import subprocess
import sys

FORMATS = ('make', 'ninja')

if sys.platform.startswith('linux'):
  test = TestGyp.TestGyp(formats=FORMATS)

  CHDIR = 'ldflags-from-environment'
  with TestGyp.LocalEnv({'LDFLAGS': '-Wl,--dynamic-linker=/target',
                         'LDFLAGS_host': '-Wl,--dynamic-linker=/host',
                         'GYP_CROSSCOMPILE': '1'}):
    test.run_gyp('test.gyp', chdir=CHDIR)
    test.build('test.gyp', chdir=CHDIR)

  def GetDynamicLinker(p):
    p = test.built_file_path(p, chdir=CHDIR)
    r = re.compile(r'\[Requesting program interpreter: ([^\]]+)\]')
    proc = subprocess.Popen(['readelf', '-l', p], stdout=subprocess.PIPE)
    o = proc.communicate()[0]
    assert not proc.returncode
    return r.search(o).group(1)

  if GetDynamicLinker('ldflags') != '/target':
    test.fail_test()

  if GetDynamicLinker('ldflags_host') != '/host':
    test.fail_test()

  test.pass_test()
