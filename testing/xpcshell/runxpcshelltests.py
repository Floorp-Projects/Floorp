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

import re, sys, os, os.path, logging, shutil, signal, math
from glob import glob
from optparse import OptionParser
from subprocess import Popen, PIPE, STDOUT
from tempfile import mkdtemp, gettempdir

from automationutils import *

""" Control-C handling """
gotSIGINT = False
def markGotSIGINT(signum, stackFrame):
  global gotSIGINT 
  gotSIGINT = True

class XPCShellTests(object):

  log = logging.getLogger()
  oldcwd = os.getcwd()

  def __init__(self):
    """ Init logging """
    handler = logging.StreamHandler(sys.stdout)
    self.log.setLevel(logging.INFO)
    self.log.addHandler(handler)

  def readManifest(self):
    """
      For a given manifest file, read the contents and populate self.testdirs
    """
    manifestdir = os.path.dirname(self.manifest)
    try:
      f = open(self.manifest, "r")
      for line in f:
        path = os.path.join(manifestdir, line.rstrip())
        if os.path.isdir(path):
          self.testdirs.append(path)
      f.close()
    except:
      pass # just eat exceptions

  def buildTestList(self):
    """
      Builds a dict of {"testdir" : ["testfile1", "testfile2", ...], "testdir2"...}.
      If manifest is given override testdirs to build initial list of directories and tests.
      If testpath is given, use that, otherwise chunk if requested.
      The resulting set of tests end up in self.alltests
    """
    self.buildTestPath()

    self.alltests = {}
    if self.manifest is not None:
      self.readManifest()

    for dir in self.testdirs:
      tests = self.getTestFiles(dir)
      if tests:
        self.alltests[os.path.abspath(dir)] = tests

    if self.singleFile is None and self.totalChunks > 1:
      self.chunkTests()

  def chunkTests(self):
    """
      Split the list of tests up into [totalChunks] pieces and filter the
      self.alltests based on thisChunk, so we only run a subset.
    """
    totalTests = 0
    for dir in self.alltests:
      totalTests += len(self.alltests[dir])

    testsPerChunk = math.ceil(totalTests / float(self.totalChunks))
    start = int(round((self.thisChunk-1) * testsPerChunk))
    end = start + testsPerChunk
    currentCount = 0

    templist = {}
    for dir in self.alltests:
      startPosition = 0
      dirCount = len(self.alltests[dir])
      endPosition = dirCount
      if currentCount < start and currentCount + dirCount >= start:
        startPosition = int(start - currentCount)        
      if currentCount + dirCount > end:
        endPosition = int(end - currentCount)
      if end - currentCount < 0 or (currentCount + dirCount < start):
        endPosition = 0

      if startPosition is not endPosition:
        templist[dir] = self.alltests[dir][startPosition:endPosition]
      currentCount += dirCount

    self.alltests = templist

  def setAbsPath(self):
    """
      Set the absolute path for xpcshell, httpdjspath and xrepath.
      These 3 variables depend on input from the command line and we need to allow for absolute paths.
      This function is overloaded for a remote solution as os.path* won't work remotely.
    """
    self.testharnessdir = os.path.dirname(os.path.abspath(__file__))
    self.headJSPath = self.testharnessdir.replace("\\", "/") + "/head.js"
    self.xpcshell = os.path.abspath(self.xpcshell)

    # we assume that httpd.js lives in components/ relative to xpcshell
    self.httpdJSPath = os.path.join(os.path.dirname(self.xpcshell), 'components', 'httpd.js')
    self.httpdJSPath = replaceBackSlashes(self.httpdJSPath)

    self.httpdManifest = os.path.join(os.path.dirname(self.xpcshell), 'components', 'httpd.manifest')
    self.httpdManifest = replaceBackSlashes(self.httpdManifest)

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
    elif sys.platform == 'osx' or sys.platform == "darwin":
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
      self.xpcsRunArgs = ['-e', '_execute_test(); quit(0);']

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
    # - NOTE: if you rename/add any of the constants set here, update
    #   do_load_child_test_harness() in head.js
    self.xpcsCmd = [self.xpcshell, '-g', self.xrePath, '-r', self.httpdManifest, '-j', '-s'] + \
        ['-e', 'const _HTTPD_JS_PATH = "%s";' % self.httpdJSPath,
         '-e', 'const _HEAD_JS_PATH = "%s";' % self.headJSPath,
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
    if self.testPath is not None:
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
    testfiles = sorted(glob(os.path.join(os.path.abspath(testdir), "test_*.js")))
    if self.singleFile:
      if self.singleFile in [os.path.basename(x) for x in testfiles]:
        testfiles = [os.path.abspath(os.path.join(testdir, self.singleFile))]
      else: # not in this dir? skip it
        return None
            
    return testfiles

  def setupProfileDir(self):
    """
      Create a temporary folder for the profile and set appropriate environment variables.
      When running check-interactive and check-one, the directory is well-defined and
      retained for inspection once the tests complete.

      On a remote system, we overload this to use a remote path structure.
    """
    if self.interactive or self.singleFile:
      profileDir = os.path.join(gettempdir(), self.profileName, "xpcshellprofile")
      try:
        # This could be left over from previous runs
        self.removeDir(profileDir)
      except:
        pass
      os.makedirs(profileDir)
    else:
      profileDir = mkdtemp()
    self.env["XPCSHELL_TEST_PROFILE_DIR"] = profileDir
    if self.interactive or self.singleFile:
      print "TEST-INFO | profile dir is %s" % profileDir
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

  def createLogFile(self, test, stdout, leakLogs):
    """
      For a given test and stdout buffer, create a log file.  also log any found leaks.
      On a remote system we have to fix the test name since it can contain directories.
    """
    try:
      f = open(test + ".log", "w")
      f.write(stdout)

      for leakLog in leakLogs: 
        if os.path.exists(leakLog):
          leaks = open(leakLog, "r")
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
               interactive=False, verbose=False, keepGoing=False, logfiles=True,
               thisChunk=1, totalChunks=1, debugger=None,
               debuggerArgs=None, debuggerInteractive=False,
               profileName=None):
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
    |verbose|, if set to True, will cause stdout/stderr from tests to
      be printed always
    |logfiles|, if set to False, indicates not to save output to log files.
      Non-interactive only option.
    |debuggerInfo|, if set, specifies the debugger and debugger arguments
      that will be used to launch xpcshell.
    |profileName|, if set, specifies the name of the application for the profile
      directory if running only a subset of tests
    """

    global gotSIGINT 

    self.xpcshell = xpcshell
    self.xrePath = xrePath
    self.symbolsPath = symbolsPath
    self.manifest = manifest
    self.testdirs = testdirs
    self.testPath = testPath
    self.interactive = interactive
    self.verbose = verbose
    self.keepGoing = keepGoing
    self.logfiles = logfiles
    self.totalChunks = totalChunks
    self.thisChunk = thisChunk
    self.debuggerInfo = getDebuggerInfo(self.oldcwd, debugger, debuggerArgs, debuggerInteractive)
    self.profileName = profileName or "xpcshell"

    # If we have an interactive debugger, disable ctrl-c.
    if self.debuggerInfo and self.debuggerInfo["interactive"]:
        signal.signal(signal.SIGINT, lambda signum, frame: None)

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

    self.buildTestList()

    for testdir in sorted(self.alltests.keys()):
      if self.testPath and not testdir.endswith(self.testPath):
        continue

      self.buildXpcsCmd(testdir)
      testHeadFiles = self.getHeadFiles(testdir)
      testTailFiles = self.getTailFiles(testdir)
      cmdH = self.buildCmdHead(testHeadFiles, testTailFiles, self.xpcsCmd)

      # Now execute each test individually.
      for test in self.alltests[testdir]:
        # create a temp dir that the JS harness can stick a profile in
        self.profileDir = self.setupProfileDir()
        self.leakLogFile = self.setupLeakLogging()

        # The test file will have to be loaded after the head files.
        cmdT = ['-e', 'const _TEST_FILE = ["%s"];' %
                replaceBackSlashes(test)]

        try:
          print "TEST-INFO | %s | running test ..." % test

          proc = self.launchProcess(cmdH + cmdT + self.xpcsRunArgs,
                      stdout=pStdout, stderr=pStderr, env=self.env, cwd=testdir)

          # Allow user to kill hung subprocess with SIGINT w/o killing this script
          # - don't move this line above launchProcess, or child will inherit the SIG_IGN
          signal.signal(signal.SIGINT, markGotSIGINT)
          # |stderr == None| as |pStderr| was either |None| or redirected to |stdout|.
          stdout, stderr = self.communicate(proc)
          signal.signal(signal.SIGINT, signal.SIG_DFL)

          if interactive:
            # Not sure what else to do here...
            return True

          if (self.getReturnCode(proc) != 0) or \
              (stdout and re.search("^((parent|child): )?TEST-UNEXPECTED-FAIL", stdout, re.MULTILINE)) or \
              (stdout and re.search(": SyntaxError:", stdout, re.MULTILINE)):
            print """TEST-UNEXPECTED-FAIL | %s | test failed (with xpcshell return code: %d), see following log:
  >>>>>>>
  %s
  <<<<<<<""" % (test, self.getReturnCode(proc), stdout)
            failCount += 1
          else:
            print "TEST-PASS | %s | test passed" % test
            if verbose:
              print """>>>>>>>\n%s\n<<<<<<<""" % stdout
            passCount += 1

          checkForCrashes(testdir, self.symbolsPath, testName=test)
          # Find child process(es) leak log(s), if any: See InitLog() in
          # xpcom/base/nsTraceRefcntImpl.cpp for logfile naming logic
          leakLogs = [self.leakLogFile]
          for childLog in glob(os.path.join(self.profileDir, "runxpcshelltests_leaks_*_pid*.log")):
            if os.path.isfile(childLog):
              leakLogs += [childLog]
          for log in leakLogs:
            dumpLeakLog(log, True)

          if self.logfiles and stdout:
            self.createLogFile(test, stdout, leakLogs)
        finally:
          # We don't want to delete the profile when running check-interactive
          # or check-one.
          if self.profileDir and not self.interactive and not self.singleFile:
            self.removeDir(self.profileDir)
        if gotSIGINT:
          print "TEST-UNEXPECTED-FAIL | Received SIGINT (control-C) during test execution"
          if (keepGoing):
            gotSIGINT = False
          else:
            break
    if passCount == 0 and failCount == 0:
      print "TEST-UNEXPECTED-FAIL | runxpcshelltests.py | No tests run. Did you pass an invalid --test-path?"
      failCount = 1

    print """INFO | Result summary:
INFO | Passed: %d
INFO | Failed: %d""" % (passCount, failCount)

    if gotSIGINT and not keepGoing:
      print "TEST-UNEXPECTED-FAIL | Received SIGINT (control-C), so stopped run. " \
            "(Use --keep-going to keep running tests after killing one with SIGINT)"
      return False
    return failCount == 0

class XPCShellOptions(OptionParser):
  def __init__(self):
    """Process command line arguments and call runTests() to do the real work."""
    OptionParser.__init__(self)

    addCommonOptions(self)
    self.add_option("--interactive",
                    action="store_true", dest="interactive", default=False,
                    help="don't automatically run tests, drop to an xpcshell prompt")
    self.add_option("--verbose",
                    action="store_true", dest="verbose", default=False,
                    help="always print stdout and stderr from tests")
    self.add_option("--keep-going",
                    action="store_true", dest="keepGoing", default=False,
                    help="continue running tests after test killed with control-C (SIGINT)")
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
    self.add_option("--total-chunks",
                    type = "int", dest = "totalChunks", default=1,
                    help = "how many chunks to split the tests up into")
    self.add_option("--this-chunk",
                    type = "int", dest = "thisChunk", default=1,
                    help = "which chunk to run between 1 and --total-chunks")
    self.add_option("--profile-name",
                    type = "string", dest="profileName", default=None,
                    help="name of application profile being tested")

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

  if options.interactive and not options.testPath:
    print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
    sys.exit(1)

  if not xpcsh.runTests(args[0], testdirs=args[1:], **options.__dict__):
    sys.exit(1)

if __name__ == '__main__':
  main()
