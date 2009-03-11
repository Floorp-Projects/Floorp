#!/usr/bin/env python
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is The Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Ted Mielczarek <ted.mielczarek@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** */

import sys, os, os.path
from glob import glob
from optparse import OptionParser
from subprocess import Popen, PIPE, STDOUT

def runTests(xpcshell, topsrcdir, testdirs, xrePath=None, testFile=None, interactive=False):
  """Run the tests in |testdirs| using the |xpcshell| executable.
  If provided, |xrePath| is the path to the XRE to use. If provided,
  |testFile| indicates a single test to run. |interactive|, if set to True,
  indicates to provide an xpcshell prompt instead of automatically executing
  the test."""
  xpcshell = os.path.abspath(xpcshell)
  env = dict(os.environ)
  env["NATIVE_TOPSRCDIR"] = os.path.normpath(topsrcdir)
  env["TOPSRCDIR"] = topsrcdir
  # Make assertions fatal
  env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"

  if xrePath is None:
    xrePath = os.path.dirname(xpcshell)
  if sys.platform == 'win32':
    env["PATH"] = env["PATH"] + ";" + xrePath
  elif sys.platform == 'osx':
    env["DYLD_LIBRARY_PATH"] = xrePath
  else: # unix or linux?
    env["LD_LIBRARY_PATH"] = xrePath
  args = [xpcshell, '-g', xrePath, '-j', '-s']

  testharnessdir = os.path.dirname(os.path.abspath(sys.argv[0]))
  headfiles = ['-f', os.path.join(testharnessdir, 'head.js')]
  tailfiles = ['-f', os.path.join(testharnessdir, 'tail.js')]
  if not interactive:
    tailfiles += ['-e', '_execute_test();']

  # when --test is specified, it can either be just a filename or
  # testdir/filename. This is for convenience when there's only one
  # test dir.
  singleDir = None
  if testFile and testFile.find('/') != -1:
    # directory was specified
    bits = testFile.split('/', 1)
    singleDir = bits[0]
    testFile = bits[1]
  for testdir in testdirs:
    if singleDir and singleDir != os.path.basename(testdir):
      continue

    # get the list of head and tail files from the directory
    testheadfiles = []
    for f in glob(os.path.join(testdir, "head_*.js")):
      if os.path.isfile(f):
        testheadfiles += ['-f', f]
    testtailfiles = []
    for f in glob(os.path.join(testdir, "tail_*.js")):
      if os.path.isfile(f):
        testtailfiles += ['-f', f]

    # now execute each test individually
    # if a single test file was specified, we only want to execute that test
    testfiles = glob(os.path.join(testdir, "test_*.js"))
    if testFile:
      if testFile in [os.path.basename(x) for x in testfiles]:
        testfiles = [os.path.join(testdir, testFile)]
      else: # not in this dir? skip it
        continue
    for test in testfiles:
      pstdout = PIPE
      pstderr = STDOUT
      interactiveargs = []
      if interactive:
        pstdout = None
        pstderr = None
        interactiveargs = ['-e', 'print("To start the test, type _execute_test();")', '-i']
      proc = Popen(args + headfiles + testheadfiles
                   + ['-f', test]
                   + tailfiles + testtailfiles + interactiveargs,
                   stdout=pstdout, stderr=pstderr, env=env)
      stdout, stderr = proc.communicate()

      if interactive:
        # not sure what else to do here...
        return True

      if proc.returncode != 0 or stdout.find("*** PASS") == -1:
        print """TEST-UNEXPECTED-FAIL | %s | test failed, see log
  %s.log:
  >>>>>>>
  %s
  <<<<<<<""" % (test, test, stdout)
        return False

      print "TEST-PASS | %s | all tests passed" % test
  return True

def main():
  """Process command line arguments and call runTests() to do the real work."""
  parser = OptionParser()
  parser.add_option("--xre-path",
                    action="store", type="string", dest="xrePath", default=None,
                    help="absolute path to directory containing XRE (probably xulrunner)")
  parser.add_option("--test",
                    action="store", type="string", dest="testFile", default=None,
                    help="single test filename to test")
  parser.add_option("--interactive",
                    action="store_true", dest="interactive", default=False,
                    help="don't automatically run tests, drop to an xpcshell prompt")
  options, args = parser.parse_args()

  if len(args) < 3:
    print >>sys.stderr, "Usage: %s <path to xpcshell> <topsrcdir> <test dirs>" % sys.argv[0]
    sys.exit(1)

  if options.interactive and not options.testFile:
    print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
    sys.exit(1)

  if not runTests(args[0], args[1], args[2:], xrePath=options.xrePath, testFile=options.testFile, interactive=options.interactive):
    sys.exit(1)

if __name__ == '__main__':
  main()
