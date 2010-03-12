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
#  Serge Gautherie <sgautherie.bz@free.fr>
#  Ted Mielczarek <ted.mielczarek@gmail.com>
#  Joel Maher <joel.maher@gmail.com>
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

import re, sys, os, os.path, logging, shutil, signal
from glob import glob
from optparse import OptionParser
from subprocess import Popen, PIPE, STDOUT
from tempfile import mkdtemp

from automationutils import *

class XPCShellTests(object):

  log = logging.getLogger()
  oldcwd = os.getcwd()

  def __init__(self):
    """ Init logging """
    handler = logging.StreamHandler(sys.stdout)
    self.log.setLevel(logging.INFO)
    self.log.addHandler(handler)

  def readManifest(self, manifest):
    """
      Given a manifest file containing a list of test directories,
      return a list of absolute paths to the directories contained within.
    """
    manifestdir = os.path.dirname(manifest)
    testdirs = []
    try:
      f = open(manifest, "r")
      for line in f:
        dir = line.rstrip()
        path = os.path.join(manifestdir, dir)
        if os.path.isdir(path):
          testdirs.append(path)
      f.close()
    except:
      pass # just eat exceptions
    return testdirs

  def setAbsPath(self):
    """
      Set the absolute path for xpcshell, httpdjspath and xrepath.
      These 3 variables depend on input from the command line and we need to allow for absolute paths.
      This function is overloaded for a remote solution as os.path* won't work remotely.
    """
    self.testharnessdir = os.path.dirname(os.path.abspath(__file__))
    self.xpcshell = os.path.abspath(self.xpcshell)

    # we assume that httpd.js lives in components/ relative to xpcshell
    self.httpdJSPath = os.path.join(os.path.dirname(self.xpcshell), 'components', 'httpd.js')
    self.httpdJSPath = replaceBackSlashes(self.httpdJSPath)

    if self.xrePath is None:
      self.xrePath = os.path.dirname(self.xpcshell)
    else:
      self.xrePath = os.path.abspath(self.xrePath)
            
  def buildEnvironment(self):
    """
      Create and returns a dictionary of self.env to include all the appropriate env variables and values.
      On a remote system, we overload this to set different values and are missing things like os.environ and PATH.
    """
    self.env = dict(os.environ)
    # Make assertions fatal
    self.env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
    # Don't launch the crash reporter client
    self.env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"

    if sys.platform == 'win32':
      self.env["PATH"] = self.env["PATH"] + ";" + self.xrePath
    elif sys.platform in ('os2emx', 'os2knix'):
      os.environ["BEGINLIBPATH"] = self.xrePath + ";" + self.env["BEGINLIBPATH"]
      os.environ["LIBPATHSTRICT"] = "T"
    elif sys.platform == 'osx':
      self.env["DYLD_LIBRARY_PATH"] = self.xrePath
    else: # unix or linux?
      self.env["LD_LIBRARY_PATH"] = self.xrePath
    return self.env

  def buildXpcsRunArgs(self):
    """
      Add arguments to run the test or make it interactive.
    """
    if self.interactive:
      self.xpcsRunArgs = [
      '-e', 'print("To start the test, type |_execute_test();|.");',
      '-i']
    else:
      self.xpcsRunArgs = ['-e', '_execute_test();']

  def getPipes(self):
    """
      Determine the value of the stdout and stderr for the test.
      Return value is a list (pStdout, pStderr).
    """
    if self.interactive:
      pStdout = None
      pStderr = None
    else:
      if (self.debuggerInfo and self.debuggerInfo["interactive"]):
        pStdout = None
        pStderr = None
      else:
        if sys.platform == 'os2emx':
          pStdout = None
        else:
          pStdout = PIPE
        pStderr = STDOUT
    return pStdout, pStderr

  def buildXpcsCmd(self, testdir):
    """
      Load the root head.js file as the first file in our test path, before other head, test, and tail files.
      On a remote system, we overload this to add additional command line arguments, so this gets overloaded.
    """
    self.xpcsCmd = [self.xpcshell, '-g', self.xrePath, '-j', '-s'] + \
        ['-e', 'const _HTTPD_JS_PATH = "%s";' % self.httpdJSPath,
        '-f', os.path.join(self.testharnessdir, 'head.js')]

    if self.debuggerInfo:
      self.xpcsCmd = [self.debuggerInfo["path"]] + self.debuggerInfo["args"] + self.xpcsCmd

  def buildTestPath(self):
    """
      If we specifiy a testpath, set the self.testPath variable to be the given directory or file.

      |testPath| will be the optional path only, or |None|.
      |singleFile| will be the optional test only, or |None|.
    """
    self.singleFile = None
    if self.testPath:
      if self.testPath.endswith('.js'):
        # Split into path and file.
        if self.testPath.find('/') == -1:
          # Test only.
          self.singleFile = self.testPath
          self.testPath = None
        else:
          # Both path and test.
          # Reuse |testPath| temporarily.
          self.testPath = self.testPath.rsplit('/', 1)
          self.singleFile = self.testPath[1]
          self.testPath = self.testPath[0]
      else:
        # Path only.
        # Simply remove optional ending separator.
        self.testPath = self.testPath.rstrip("/")

  def getHeadFiles(self, testdir):
    """
      Get the list of head files for a given test directory.
      On a remote system, this is overloaded to list files in a remote directory structure.
    """
    return [f for f in sorted(glob(os.path.join(testdir, "head_*.js"))) if os.path.isfile(f)]

  def getTailFiles(self, testdir):
    """
      Get the list of tail files for a given test directory.
      Tails are executed in the reverse order, to "match" heads order,
      as in "h1-h2-h3 then t3-t2-t1".

      On a remote system, this is overloaded to list files in a remote directory structure.
    """
    return [f for f in reversed(sorted(glob(os.path.join(testdir, "tail_*.js")))) if os.path.isfile(f)]

  def getTestFiles(self, testdir):
    """
      Ff a single test file was specified, we only want to execute that test,
      otherwise return a list of all tests in a directory

      On a remote system, this is overloaded to find files in the remote directory structure.
    """
    testfiles = sorted(glob(os.path.join(testdir, "test_*.js")))
    if self.singleFile:
      if singleFile in [os.path.basename(x) for x in testfiles]:
        testfiles = [os.path.join(testdir, singleFile)]
      else: # not in this dir? skip it
        return None
            
    return testfiles

  def setupProfileDir(self):
    """
      Create a temporary folder for the profile and set appropriate environment variables.

      On a remote system, we overload this to use a remote path structure.
    """
    profileDir = mkdtemp()
    self.env["XPCSHELL_TEST_PROFILE_DIR"] = profileDir
    return profileDir

  def setupLeakLogging(self):
    """
      Enable leaks (only) detection to its own log file and set environment variables.

      On a remote system, we overload this to use a remote filename and path structure
    """
    filename = "runxpcshelltests_leaks.log"

    leakLogFile = os.path.join(self.profileDir,  filename)
    self.env["XPCOM_MEM_LEAK_LOG"] = leakLogFile
    return leakLogFile

  def launchProcess(self, cmd, stdout, stderr, env, cwd):
    """
      Simple wrapper to launch a process.
      On a remote system, this is more complex and we need to overload this function.
    """
    proc = Popen(cmd, stdout=stdout, stderr=stderr, 
                env=env, cwd=cwd)
    return proc

  def communicate(self, proc):
    """
      Simple wrapper to communicate with a process.
      On a remote system, this is overloaded to handle remote process communication.
    """
    return proc.communicate()
        
  def removeDir(self, dirname):
    """
      Simple wrapper to remove (recursively) a given directory.
      On a remote system, we need to overload this to work on the remote filesystem.
    """
    shutil.rmtree(dirname)

  def verifyDirPath(self, dirname):
    """
      Simple wrapper to get the absolute path for a given directory name.
      On a remote system, we need to overload this to work on the remote filesystem.
    """
    return os.path.abspath(dirname)
        
  def getReturnCode(self, proc):
    """
      Simple wrapper to get the return code for a given process.
      On a remote system we overload this to work with the remote process management.
    """
    return proc.returncode

  def createLogFile(self, test, stdout):
    """
      For a given test and stdout buffer, create a log file.  also log any found leaks.
      On a remote system we have to fix the test name since it can contain directories.
    """
    try:
      f = open(test + ".log", "w")
      f.write(stdout)

      if os.path.exists(self.leakLogFile):
        leaks = open(self.leakLogFile, "r")
        f.write(leaks.read())
        leaks.close()
    finally:
      if f:
        f.close()

  def buildCmdHead(self, headfiles, tailfiles, xpcscmd):
    """
      Build the command line arguments for the head and tail files,
      along with the address of the webserver which some tests require.

      On a remote system, this is overloaded to resolve quoting issues over a secondary command line.
    """
    cmdH = ", ".join(['"' + replaceBackSlashes(f) + '"'
                   for f in headfiles])
    cmdT = ", ".join(['"' + replaceBackSlashes(f) + '"'
                   for f in tailfiles])
    return xpcscmd + \
            ['-e', 'const _SERVER_ADDR = "localhost"',
             '-e', 'const _HEAD_FILES = [%s];' % cmdH,
             '-e', 'const _TAIL_FILES = [%s];' % cmdT]

  def runTests(self, xpcshell, xrePath=None, symbolsPath=None,
               manifest=None, testdirs=[], testPath=None,
               interactive=False, logfiles=True,
               debuggerInfo=None):
    """Run xpcshell tests.

    |xpcshell|, is the xpcshell executable to use to run the tests.
    |xrePath|, if provided, is the path to the XRE to use.
    |symbolsPath|, if provided is the path to a directory containing
      breakpad symbols for processing crashes in tests.
    |manifest|, if provided, is a file containing a list of
      test directories to run.
    |testdirs|, if provided, is a list of absolute paths of test directories.
      No-manifest only option.
    |testPath|, if provided, indicates a single path and/or test to run.
    |interactive|, if set to True, indicates to provide an xpcshell prompt
      instead of automatically executing the test.
    |logfiles|, if set to False, indicates not to save output to log files.
      Non-interactive only option.
    |debuggerInfo|, if set, specifies the debugger and debugger arguments
      that will be used to launch xpcshell.
    """

    self.xpcshell = xpcshell
    self.xrePath = xrePath
    self.symbolsPath = symbolsPath
    self.manifest = manifest
    self.testdirs = testdirs
    self.testPath = testPath
    self.interactive = interactive
    self.logfiles = logfiles
    self.debuggerInfo = debuggerInfo

    if not testdirs and not manifest:
      # nothing to test!
      print >>sys.stderr, "Error: No test dirs or test manifest specified!"
      return False

    passCount = 0
    failCount = 0

    self.setAbsPath()
    self.buildXpcsRunArgs()
    self.buildEnvironment()
    pStdout, pStderr = self.getPipes()

    # Override testdirs.
    if manifest is not None:
      testdirs = self.readManifest(os.path.abspath(manifest))

    self.buildTestPath()

    # Process each test directory individually.
    for testdir in testdirs:
      self.buildXpcsCmd(testdir)

      if testPath and not testdir.endswith(testPath):
        continue

      testdir = os.path.abspath(testdir)

      testHeadFiles = self.getHeadFiles(testdir)
      testTailFiles = self.getTailFiles(testdir)

      testfiles = self.getTestFiles(testdir)
      if testfiles == None:
        continue

      cmdH = self.buildCmdHead(testHeadFiles, testTailFiles, self.xpcsCmd)

      # Now execute each test individually.
      for test in testfiles:
        # create a temp dir that the JS harness can stick a profile in
        self.profileDir = self.setupProfileDir()
        self.leakLogFile = self.setupLeakLogging()

        # The test file will have to be loaded after the head files.
        cmdT = ['-e', 'const _TEST_FILE = ["%s"];' %
                replaceBackSlashes(os.path.join(testdir, test))]

        try:
          proc = self.launchProcess(cmdH + cmdT + self.xpcsRunArgs,
                      stdout=pStdout, stderr=pStderr, env=self.env, cwd=testdir)

          # allow user to kill hung subprocess with SIGINT w/o killing this script
          # - don't move this line above Popen, or child will inherit the SIG_IGN
          signal.signal(signal.SIGINT, signal.SIG_IGN)
          # |stderr == None| as |pStderr| was either |None| or redirected to |stdout|.
          stdout, stderr = self.communicate(proc)
          signal.signal(signal.SIGINT, signal.SIG_DFL)

          if interactive:
            # Not sure what else to do here...
            return True

          if (self.getReturnCode(proc) != 0) or (stdout and re.search("^TEST-UNEXPECTED-FAIL", stdout, re.MULTILINE)):
            print """TEST-UNEXPECTED-FAIL | %s | test failed (with xpcshell return code: %d), see following log:
  >>>>>>>
  %s
  <<<<<<<""" % (test, self.getReturnCode(proc), stdout)
            failCount += 1
          else:
            print "TEST-PASS | %s | test passed" % test
            passCount += 1

          checkForCrashes(testdir, self.symbolsPath, testName=test)
          dumpLeakLog(self.leakLogFile, True)

          if self.logfiles and stdout:
            self.createLogFile(test, stdout)
        finally:
          if self.profileDir:
            self.removeDir(self.profileDir)

    if passCount == 0 and failCount == 0:
      print "TEST-UNEXPECTED-FAIL | runxpcshelltests.py | No tests run. Did you pass an invalid --test-path?"
      failCount = 1

    print """INFO | Result summary:
INFO | Passed: %d
INFO | Failed: %d""" % (passCount, failCount)

    return failCount == 0

class XPCShellOptions(OptionParser):
  def __init__(self):
    """Process command line arguments and call runTests() to do the real work."""
    OptionParser.__init__(self)

    addCommonOptions(self)
    self.add_option("--interactive",
                    action="store_true", dest="interactive", default=False,
                    help="don't automatically run tests, drop to an xpcshell prompt")
    self.add_option("--logfiles",
                    action="store_true", dest="logfiles", default=True,
                    help="create log files (default, only used to override --no-logfiles)")
    self.add_option("--manifest",
                    type="string", dest="manifest", default=None,
                    help="Manifest of test directories to use")
    self.add_option("--no-logfiles",
                    action="store_false", dest="logfiles",
                    help="don't create log files")
    self.add_option("--test-path",
                    type="string", dest="testPath", default=None,
                    help="single path and/or test filename to test")

def main():
  parser = XPCShellOptions()
  options, args = parser.parse_args()

  if len(args) < 2 and options.manifest is None or \
     (len(args) < 1 and options.manifest is not None):
     print >>sys.stderr, """Usage: %s <path to xpcshell> <test dirs>
           or: %s --manifest=test.manifest <path to xpcshell>""" % (sys.argv[0],
                                                           sys.argv[0])
     sys.exit(1)

  xpcsh = XPCShellTests()
  debuggerInfo = getDebuggerInfo(xpcsh.oldcwd, options.debugger, options.debuggerArgs,
    options.debuggerInteractive);

  if options.interactive and not options.testPath:
    print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
    sys.exit(1)

    
  if not xpcsh.runTests(args[0],
                        xrePath=options.xrePath,
                        symbolsPath=options.symbolsPath,
                        manifest=options.manifest,
                        testdirs=args[1:],
                        testPath=options.testPath,
                        interactive=options.interactive,
                        logfiles=options.logfiles,
                        debuggerInfo=debuggerInfo):
    sys.exit(1)

if __name__ == '__main__':
  main()
