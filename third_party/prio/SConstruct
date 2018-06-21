# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import SCons

opts = Variables()
opts.AddVariables(
    BoolVariable("DEBUG", "Make debug build", 1),
    BoolVariable("VERBOSE", "Show full build information", 0))

env = Environment(options = opts,
                  ENV = os.environ)
if "CFLAGS" in os.environ:
  env.Append(CFLAGS = SCons.Util.CLVar(os.getenv("CFLAGS")))
if "CPPFLAGS" in os.environ:
  env.Append(CPPFLAGS = SCons.Util.CLVar(os.getenv("CPPFLAGS")))
if "CXXFLAGS" in os.environ:
  env.Append(CXXFLAGS = SCons.Util.CLVar(os.getenv("CXXFLAGS")))
if "LDFLAGS" in os.environ:
  env.Append(LINKFLAGS = SCons.Util.CLVar(os.getenv("LDFLAGS")))

if env["DEBUG"]: 
    print "DEBUG MODE!"
    env.Append(CPPFLAGS = [ "-g", "-DDEBUG"])

env.Append(LIBS = ["mprio", "mpi", "nss3", "nspr4"], \
  LIBPATH = ['#build/prio', "#build/mpi"],
  CFLAGS = [ "-Wall", "-Werror", "-Wextra", "-O3", "-std=c99", 
    "-I/usr/include/nspr", "-Impi", "-DDO_PR_CLEANUP"])

env.Append(CPPPATH = ["#include", "#."])
Export('env')

SConscript('browser-test/SConscript', variant_dir='build/browser-test')
SConscript('mpi/SConscript', variant_dir='build/mpi')
SConscript('pclient/SConscript', variant_dir='build/pclient')
SConscript('prio/SConscript', variant_dir='build/prio')
SConscript('ptest/SConscript', variant_dir='build/ptest')

