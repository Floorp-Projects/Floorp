#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compiler version checking tool for gcc

Print gcc version as XY if you are running gcc X.Y.*.
This is used to tweak build flags for gcc 4.4.
"""

import os
import re
import subprocess
import sys

def GetVersion(compiler):
  try:
    # Note that compiler could be something tricky like "distcc g++".
    compiler = compiler + " -dumpversion"
    pipe = subprocess.Popen(compiler, stdout=subprocess.PIPE, shell=True)
    gcc_output = pipe.communicate()[0]
    result = re.match(r"(\d+)\.(\d+)", gcc_output)
    return result.group(1) + result.group(2)
  except Exception, e:
    print >> sys.stderr, "compiler_version.py failed to execute:", compiler
    print >> sys.stderr, e
    return ""

def main():
  # Check if CXX environment variable exists and
  # if it does use that compiler.
  cxx = os.getenv("CXX", None)
  if cxx:
    cxxversion = GetVersion(cxx)
    if cxxversion != "":
      print cxxversion
      return 0
  else:
    # Otherwise we check the g++ version.
    gccversion = GetVersion("g++")
    if gccversion != "":
      print gccversion
      return 0

  return 1

if __name__ == "__main__":
  sys.exit(main())
