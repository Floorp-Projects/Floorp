#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re, sys, os, os.path, logging, shutil, signal, math, time
import xml.dom.minidom
from glob import glob
from optparse import OptionParser
from subprocess import Popen, PIPE, STDOUT
from tempfile import mkdtemp, gettempdir
import manifestparser
import mozinfo
import random

from automationutils import *

#TODO: replace this with json.loads when Python 2.6 is required.
def parse_json(j):
    """
    Awful hack to parse a restricted subset of JSON strings into Python dicts.
    """
    return eval(j, {'true':True,'false':False,'null':None})
  
""" Control-C handling """
gotSIGINT = False
def markGotSIGINT(signum, stackFrame):
  global gotSIGINT 
  gotSIGINT = True

class XPCShellTests(object):

  log = logging.getLogger()
  oldcwd = os.getcwd()

  def __init__(self, log=sys.stdout):
    """ Init logging and node status """
    handler = logging.StreamHandler(log)
    self.log.setLevel(logging.INFO)
    self.log.addHandler(handler)
    self.nodeProc = None

  def buildTestList(self):
    """
      read the xpcshell.ini manifest and set self.alltests to be 
      an array of test objects.

      if we are chunking tests, it will be done here as well
    """
    mp = manifestparser.TestManifest(strict=False)
    if self.manifest is None:
      for testdir in self.testdirs:
        if testdir:
          mp.read(os.path.join(testdir, 'xpcshell.ini'))
    else:
      mp.read(self.manifest)
    self.buildTestPath()

    self.alltests = mp.active_tests(**mozinfo.info)

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

    if self.mozInfo is None:
      self.mozInfo = os.path.join(self.testharnessdir, "mozinfo.json")

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
    # Capturing backtraces is very slow on some platforms, and it's
    # disabled by automation.py too
    self.env["NS_TRACE_MALLOC_DISABLE_STACKS"] = "1"

    if sys.platform == 'win32':
      self.env["PATH"] = self.env["PATH"] + ";" + self.xrePath
    elif sys.platform in ('os2emx', 'os2knix'):
      os.environ["BEGINLIBPATH"] = self.xrePath + ";" + self.env["BEGINLIBPATH"]
      os.environ["LIBPATHSTRICT"] = "T"
    elif sys.platform == 'osx' or sys.platform == "darwin":
      self.env["DYLD_LIBRARY_PATH"] = self.xrePath
    else: # unix or linux?
      if not "LD_LIBRARY_PATH" in self.env or self.env["LD_LIBRARY_PATH"] is None:
        self.env["LD_LIBRARY_PATH"] = self.xrePath
      else:
        self.env["LD_LIBRARY_PATH"] = ":".join([self.xrePath, self.env["LD_LIBRARY_PATH"]])

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
    if not self.appPath:
        self.appPath = self.xrePath

    self.xpcsCmd = [
        self.xpcshell,
        '-g', self.xrePath,
        '-a', self.appPath,
        '-r', self.httpdManifest,
        '-m',
        '-n',
        '-s',
        '-e', 'const _HTTPD_JS_PATH = "%s";' % self.httpdJSPath,
        '-e', 'const _HEAD_JS_PATH = "%s";' % self.headJSPath
    ]

    if self.testingModulesDir:
        # Escape backslashes in string literal.
        sanitized = self.testingModulesDir.replace('\\', '\\\\')
        self.xpcsCmd.extend([
            '-e',
            'const _TESTING_MODULES_DIR = "%s";' % sanitized
        ])

    self.xpcsCmd.extend(['-f', os.path.join(self.testharnessdir, 'head.js')])

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

  def getHeadAndTailFiles(self, test):
      """Obtain the list of head and tail files.

      Returns a 2-tuple. The first element is a list of head files. The second
      is a list of tail files.
      """
      def sanitize_list(s, kind):
          for f in s.strip().split(' '):
              f = f.strip()
              if len(f) < 1:
                  continue

              path = os.path.normpath(os.path.join(test['here'], f))
              if not os.path.exists(path):
                  raise Exception('%s file does not exist: %s' % (kind, path))

              if not os.path.isfile(path):
                  raise Exception('%s file is not a file: %s' % (kind, path))

              yield path

      return (list(sanitize_list(test['head'], 'head')),
              list(sanitize_list(test['tail'], 'tail')))

  def setupProfileDir(self):
    """
      Create a temporary folder for the profile and set appropriate environment variables.
      When running check-interactive and check-one, the directory is well-defined and
      retained for inspection once the tests complete.

      On a remote system, this may be overloaded to use a remote path structure.
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
      self.log.info("TEST-INFO | profile dir is %s" % profileDir)
    return profileDir

  def setupLeakLogging(self):
    """
      Enable leaks (only) detection to its own log file and set environment variables.

      On a remote system, this may be overloaded to use a remote filename and path structure
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
    cmd = wrapCommand(cmd)
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

  def buildCmdTestFile(self, name):
    """
      Build the command line arguments for the test file.
      On a remote system, this may be overloaded to use a remote path structure.
    """
    return ['-e', 'const _TEST_FILE = ["%s"];' %
              replaceBackSlashes(name)]

  def trySetupNode(self):
    """
      Run node for SPDY tests, if available, and updates mozinfo as appropriate.
    """
    nodeMozInfo = {'hasNode': False} # Assume the worst
    nodeBin = None

    # We try to find the node executable in the path given to us by the user in
    # the MOZ_NODE_PATH environment variable
    localPath = os.getenv('MOZ_NODE_PATH', None)
    if localPath and os.path.exists(localPath) and os.path.isfile(localPath):
      nodeBin = localPath

    if nodeBin:
      self.log.info('Found node at %s' % (nodeBin,))
      myDir = os.path.split(os.path.abspath(__file__))[0]
      mozSpdyJs = os.path.join(myDir, 'moz-spdy', 'moz-spdy.js')

      if os.path.exists(mozSpdyJs):
        # OK, we found our SPDY server, let's try to get it running
        self.log.info('Found moz-spdy at %s' % (mozSpdyJs,))
        stdout, stderr = self.getPipes()
        try:
          # We pipe stdin to node because the spdy server will exit when its
          # stdin reaches EOF
          self.nodeProc = Popen([nodeBin, mozSpdyJs], stdin=PIPE, stdout=PIPE,
                  stderr=STDOUT, env=self.env, cwd=os.getcwd())

          # Check to make sure the server starts properly by waiting for it to
          # tell us it's started
          msg = self.nodeProc.stdout.readline()
          if msg.startswith('SPDY server listening'):
              nodeMozInfo['hasNode'] = True
        except OSError, e:
          # This occurs if the subprocess couldn't be started
          self.log.error('Could not run node SPDY server: %s' % (str(e),))

    mozinfo.update(nodeMozInfo)

  def shutdownNode(self):
    """
      Shut down our node process, if it exists
    """
    if self.nodeProc:
      self.log.info('Node SPDY server shutting down ...')
      # moz-spdy exits when its stdin reaches EOF, so force that to happen here
      self.nodeProc.communicate()

  def writeXunitResults(self, results, name=None, filename=None, fh=None):
    """
      Write Xunit XML from results.

      The function receives an iterable of results dicts. Each dict must have
      the following keys:

        classname - The "class" name of the test.
        name - The simple name of the test.

      In addition, it must have one of the following saying how the test
      executed:

        passed - Boolean indicating whether the test passed. False if it
          failed.
        skipped - True if the test was skipped.

      The following keys are optional:

        time - Execution time of the test in decimal seconds.
        failure - Dict describing test failure. Requires keys:
          type - String type of failure.
          message - String describing basic failure.
          text - Verbose string describing failure.

      Arguments:

      |name|, Name of the test suite. Many tools expect Java class dot notation
        e.g. dom.simple.foo. A directory with '/' converted to '.' is a good
        choice.
      |fh|, File handle to write XML to.
      |filename|, File name to write XML to.
      |results|, Iterable of tuples describing the results.
    """
    if filename is None and fh is None:
      raise Exception("One of filename or fh must be defined.")

    if name is None:
      name = "xpcshell"
    else:
      assert isinstance(name, str)

    if filename is not None:
      fh = open(filename, 'wb')

    doc = xml.dom.minidom.Document()
    testsuite = doc.createElement("testsuite")
    testsuite.setAttribute("name", name)
    doc.appendChild(testsuite)

    total = 0
    passed = 0
    failed = 0
    skipped = 0

    for result in results:
      total += 1

      if result.get("skipped", None):
        skipped += 1
      elif result["passed"]:
        passed += 1
      else:
        failed += 1

      testcase = doc.createElement("testcase")
      testcase.setAttribute("classname", result["classname"])
      testcase.setAttribute("name", result["name"])

      if "time" in result:
        testcase.setAttribute("time", str(result["time"]))
      else:
        # It appears most tools expect the time attribute to be present.
        testcase.setAttribute("time", "0")

      if "failure" in result:
        failure = doc.createElement("failure")
        failure.setAttribute("type", str(result["failure"]["type"]))
        failure.setAttribute("message", result["failure"]["message"])

        # Lossy translation but required to not break CDATA. Also, text could
        # be None and Python 2.5's minidom doesn't accept None. Later versions
        # do, however.
        cdata = result["failure"]["text"]
        if not isinstance(cdata, str):
            cdata = ""

        cdata = cdata.replace("]]>", "]] >")
        text = doc.createCDATASection(cdata)
        failure.appendChild(text)
        testcase.appendChild(failure)

      if result.get("skipped", None):
        e = doc.createElement("skipped")
        testcase.appendChild(e)

      testsuite.appendChild(testcase)

    testsuite.setAttribute("tests", str(total))
    testsuite.setAttribute("failures", str(failed))
    testsuite.setAttribute("skip", str(skipped))

    doc.writexml(fh, addindent="  ", newl="\n", encoding="utf-8")

  def runTests(self, xpcshell, xrePath=None, appPath=None, symbolsPath=None,
               manifest=None, testdirs=None, testPath=None,
               interactive=False, verbose=False, keepGoing=False, logfiles=True,
               thisChunk=1, totalChunks=1, debugger=None,
               debuggerArgs=None, debuggerInteractive=False,
               profileName=None, mozInfo=None, shuffle=False,
               testsRootDir=None, xunitFilename=None, xunitName=None,
               testingModulesDir=None, **otherOptions):
    """Run xpcshell tests.

    |xpcshell|, is the xpcshell executable to use to run the tests.
    |xrePath|, if provided, is the path to the XRE to use.
    |appPath|, if provided, is the path to an application directory.
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
      directory if running only a subset of tests.
    |mozInfo|, if set, specifies specifies build configuration information, either as a filename containing JSON, or a dict.
    |shuffle|, if True, execute tests in random order.
    |testsRootDir|, absolute path to root directory of all tests. This is used
      by xUnit generation to determine the package name of the tests.
    |xunitFilename|, if set, specifies the filename to which to write xUnit XML
      results.
    |xunitName|, if outputting an xUnit XML file, the str value to use for the
      testsuite name.
    |testingModulesDir|, if provided, specifies where JS modules reside.
      xpcshell will register a resource handler mapping this path.
    |otherOptions| may be present for the convenience of subclasses
    """

    global gotSIGINT

    if testdirs is None:
        testdirs = []

    if xunitFilename is not None or xunitName is not None:
        if not isinstance(testsRootDir, str):
            raise Exception("testsRootDir must be a str when outputting xUnit.")

        if not os.path.isabs(testsRootDir):
            testsRootDir = os.path.abspath(testsRootDir)

        if not os.path.exists(testsRootDir):
            raise Exception("testsRootDir path does not exists: %s" %
                    testsRootDir)

    # Try to guess modules directory.
    # This somewhat grotesque hack allows the buildbot machines to find the
    # modules directory without having to configure the buildbot hosts. This
    # code path should never be executed in local runs because the build system
    # should always set this argument.
    if not testingModulesDir:
        ourDir = os.path.dirname(__file__)
        possible = os.path.join(ourDir, os.path.pardir, 'modules')

        if os.path.isdir(possible):
            testingModulesDir = possible

    if testingModulesDir:
        # The resource loader expects native paths. Depending on how we were
        # invoked, a UNIX style path may sneak in on Windows. We try to
        # normalize that.
        testingModulesDir = os.path.normpath(testingModulesDir)

        if not os.path.isabs(testingModulesDir):
            testingModulesDir = os.path.abspath(testingModulesDir)

        if not testingModulesDir.endswith(os.path.sep):
            testingModulesDir += os.path.sep

    self.xpcshell = xpcshell
    self.xrePath = xrePath
    self.appPath = appPath
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
    self.mozInfo = mozInfo
    self.testingModulesDir = testingModulesDir

    # If we have an interactive debugger, disable ctrl-c.
    if self.debuggerInfo and self.debuggerInfo["interactive"]:
        signal.signal(signal.SIGINT, lambda signum, frame: None)

    if not testdirs and not manifest:
      # nothing to test!
      self.log.error("Error: No test dirs or test manifest specified!")
      return False

    self.testCount = 0
    self.passCount = 0
    self.failCount = 0
    self.todoCount = 0

    self.setAbsPath()
    self.buildXpcsRunArgs()
    self.buildEnvironment()

    # Handle filenames in mozInfo
    if not isinstance(self.mozInfo, dict):
      mozInfoFile = self.mozInfo
      if not os.path.isfile(mozInfoFile):
        self.log.error("Error: couldn't find mozinfo.json at '%s'. Perhaps you need to use --build-info-json?" % mozInfoFile)
        return False
      self.mozInfo = parse_json(open(mozInfoFile).read())
    mozinfo.update(self.mozInfo)

    # We have to do this before we build the test list so we know whether or
    # not to run tests that depend on having the node spdy server
    self.trySetupNode()
    
    pStdout, pStderr = self.getPipes()

    self.buildTestList()

    if shuffle:
      random.shuffle(self.alltests)

    xunitResults = []

    for test in self.alltests:
      name = test['path']
      if self.singleFile and not name.endswith(self.singleFile):
        continue

      if self.testPath and name.find(self.testPath) == -1:
        continue

      self.testCount += 1

      xunitResult = {"name": test["name"], "classname": "xpcshell"}
      # The xUnit package is defined as the path component between the root
      # dir and the test with path characters replaced with '.' (using Java
      # class notation).
      if testsRootDir is not None:
          testsRootDir = os.path.normpath(testsRootDir)
          if test["here"].find(testsRootDir) != 0:
              raise Exception("testsRootDir is not a parent path of %s" %
                  test["here"])
          relpath = test["here"][len(testsRootDir):].lstrip("/\\")
          xunitResult["classname"] = relpath.replace("/", ".").replace("\\", ".")

      # Check for skipped tests
      if 'disabled' in test:
        self.log.info("TEST-INFO | skipping %s | %s" %
                      (name, test['disabled']))

        xunitResult["skipped"] = True
        xunitResults.append(xunitResult)
        continue

      # Check for known-fail tests
      expected = test['expected'] == 'pass'

      testdir = os.path.dirname(name)
      self.buildXpcsCmd(testdir)
      testHeadFiles, testTailFiles = self.getHeadAndTailFiles(test)
      cmdH = self.buildCmdHead(testHeadFiles, testTailFiles, self.xpcsCmd)

      # create a temp dir that the JS harness can stick a profile in
      self.profileDir = self.setupProfileDir()
      self.leakLogFile = self.setupLeakLogging()

      # The test file will have to be loaded after the head files.
      cmdT = self.buildCmdTestFile(name)

      args = self.xpcsRunArgs[:]
      if 'debug' in test:
          args.insert(0, '-d')

      completeCmd = cmdH + cmdT + args

      try:
        self.log.info("TEST-INFO | %s | running test ..." % name)
        if verbose:
            self.log.info("TEST-INFO | %s | full command: %r" % (name, completeCmd))
            self.log.info("TEST-INFO | %s | current directory: %r" % (name, testdir))
            # Show only those environment variables that are changed from
            # the ambient environment.
            changedEnv = (set("%s=%s" % i for i in self.env.iteritems())
                          - set("%s=%s" % i for i in os.environ.iteritems()))
            self.log.info("TEST-INFO | %s | environment: %s" % (name, list(changedEnv)))
        startTime = time.time()

        proc = self.launchProcess(completeCmd,
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

        def print_stdout(stdout):
          """Print stdout line-by-line to avoid overflowing buffers."""
          self.log.info(">>>>>>>")
          if (stdout):
            for line in stdout.splitlines():
              self.log.info(line)
          self.log.info("<<<<<<<")

        result = not ((self.getReturnCode(proc) != 0) or
                      # if do_throw or do_check failed
                      (stdout and re.search("^((parent|child): )?TEST-UNEXPECTED-",
                                            stdout, re.MULTILINE)) or
                      # if syntax error in xpcshell file
                      (stdout and re.search(": SyntaxError:", stdout,
                                            re.MULTILINE)) or
                      # if e10s test started but never finished (child process crash)
                      (stdout and re.search("^child: CHILD-TEST-STARTED", 
                                            stdout, re.MULTILINE) 
                              and not re.search("^child: CHILD-TEST-COMPLETED",
                                                stdout, re.MULTILINE)))

        if result != expected:
          failureType = "TEST-UNEXPECTED-%s" % ("FAIL" if expected else "PASS")
          message = "%s | %s | test failed (with xpcshell return code: %d), see following log:" % (
                        failureType, name, self.getReturnCode(proc))
          self.log.error(message)
          print_stdout(stdout)
          self.failCount += 1
          xunitResult["passed"] = False

          xunitResult["failure"] = {
            "type": failureType,
            "message": message,
            "text": stdout
          }
        else:
          now = time.time()
          timeTaken = (now - startTime) * 1000
          xunitResult["time"] = now - startTime
          self.log.info("TEST-%s | %s | test passed (time: %.3fms)" % ("PASS" if expected else "KNOWN-FAIL", name, timeTaken))
          if verbose:
            print_stdout(stdout)

          xunitResult["passed"] = True

          if expected:
            self.passCount += 1
          else:
            self.todoCount += 1

        checkForCrashes(testdir, self.symbolsPath, testName=name)
        # Find child process(es) leak log(s), if any: See InitLog() in
        # xpcom/base/nsTraceRefcntImpl.cpp for logfile naming logic
        leakLogs = [self.leakLogFile]
        for childLog in glob(os.path.join(self.profileDir, "runxpcshelltests_leaks_*_pid*.log")):
          if os.path.isfile(childLog):
            leakLogs += [childLog]
        for log in leakLogs:
          dumpLeakLog(log, True)

        if self.logfiles and stdout:
          self.createLogFile(name, stdout, leakLogs)
      finally:
        # We don't want to delete the profile when running check-interactive
        # or check-one.
        if self.profileDir and not self.interactive and not self.singleFile:
          self.removeDir(self.profileDir)
      if gotSIGINT:
        xunitResult["passed"] = False
        xunitResult["time"] = "0.0"
        xunitResult["failure"] = {
          "type": "SIGINT",
          "message": "Received SIGINT",
          "text": "Received SIGINT (control-C) during test execution."
        }

        self.log.error("TEST-UNEXPECTED-FAIL | Received SIGINT (control-C) during test execution")
        if (keepGoing):
          gotSIGINT = False
        else:
          xunitResults.append(xunitResult)
          break

      xunitResults.append(xunitResult)

    self.shutdownNode()

    if self.testCount == 0:
      self.log.error("TEST-UNEXPECTED-FAIL | runxpcshelltests.py | No tests run. Did you pass an invalid --test-path?")
      self.failCount = 1

    self.log.info("""INFO | Result summary:
INFO | Passed: %d
INFO | Failed: %d
INFO | Todo: %d""" % (self.passCount, self.failCount, self.todoCount))

    if xunitFilename is not None:
        self.writeXunitResults(filename=xunitFilename, results=xunitResults,
                               name=xunitName)

    if gotSIGINT and not keepGoing:
      self.log.error("TEST-UNEXPECTED-FAIL | Received SIGINT (control-C), so stopped run. " \
                     "(Use --keep-going to keep running tests after killing one with SIGINT)")
      return False

    return self.failCount == 0

class XPCShellOptions(OptionParser):
  def __init__(self):
    """Process command line arguments and call runTests() to do the real work."""
    OptionParser.__init__(self)

    addCommonOptions(self)
    self.add_option("--app-path",
                    type="string", dest="appPath", default=None,
                    help="application directory (as opposed to XRE directory)")
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
    self.add_option("--tests-root-dir",
                    type="string", dest="testsRootDir", default=None,
                    help="absolute path to directory where all tests are located. this is typically $(objdir)/_tests")
    self.add_option("--testing-modules-dir",
                    dest="testingModulesDir", default=None,
                    help="Directory where testing modules are located.")
    self.add_option("--total-chunks",
                    type = "int", dest = "totalChunks", default=1,
                    help = "how many chunks to split the tests up into")
    self.add_option("--this-chunk",
                    type = "int", dest = "thisChunk", default=1,
                    help = "which chunk to run between 1 and --total-chunks")
    self.add_option("--profile-name",
                    type = "string", dest="profileName", default=None,
                    help="name of application profile being tested")
    self.add_option("--build-info-json",
                    type = "string", dest="mozInfo", default=None,
                    help="path to a mozinfo.json including information about the build configuration. defaults to looking for mozinfo.json next to the script.")
    self.add_option("--shuffle",
                    action="store_true", dest="shuffle", default=False,
                    help="Execute tests in random order")
    self.add_option("--xunit-file", dest="xunitFilename",
                    help="path to file where xUnit results will be written.")
    self.add_option("--xunit-suite-name", dest="xunitName",
                    help="name to record for this xUnit test suite. Many "
                         "tools expect Java class notation, e.g. "
                         "dom.basic.foo")

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
