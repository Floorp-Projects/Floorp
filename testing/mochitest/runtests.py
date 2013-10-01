# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the Mochitest test harness.
"""

from __future__ import with_statement
import os
import sys
SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
sys.path.insert(0, SCRIPT_DIR);

import base64
import json
import mozcrash
import mozinfo
import mozlog
import mozprocess
import mozrunner
import optparse
import re
import shutil
import signal
import tempfile
import time
import traceback
import urllib2

from automationutils import environment, getDebuggerInfo, isURL, KeyValueParseError, parseKeyValue, processLeakLog, systemMemory
from datetime import datetime
from manifestparser import TestManifest
from mochitest_options import MochitestOptions
from mozprofile import Profile, Preferences
from mozprofile.permissions import ServerLocations
from urllib import quote_plus as encodeURIComponent

# This should use the `which` module already in tree, but it is
# not yet present in the mozharness environment
from mozrunner.utils import findInPath as which

# set up logging handler a la automation.py.in for compatability
import logging
log = logging.getLogger()
def resetGlobalLog():
  while log.handlers:
    log.removeHandler(log.handlers[0])
  handler = logging.StreamHandler(sys.stdout)
  log.setLevel(logging.INFO)
  log.addHandler(handler)
resetGlobalLog()

####################
# PROCESS HANDLING #
####################

def call(*args, **kwargs):
  """front-end function to mozprocess.ProcessHandler"""
  # TODO: upstream -> mozprocess
  # https://bugzilla.mozilla.org/show_bug.cgi?id=791383
  process = mozprocess.ProcessHandler(*args, **kwargs)
  process.run()
  return process.wait()

def killPid(pid):
  # see also https://bugzilla.mozilla.org/show_bug.cgi?id=911249#c58
  try:
    os.kill(pid, getattr(signal, "SIGKILL", signal.SIGTERM))
  except Exception, e:
    log.info("Failed to kill process %d: %s", pid, str(e))

if mozinfo.isWin:
  import ctypes, ctypes.wintypes, time, msvcrt

  def isPidAlive(pid):
    STILL_ACTIVE = 259
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    pHandle = ctypes.windll.kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid)
    if not pHandle:
      return False
    pExitCode = ctypes.wintypes.DWORD()
    ctypes.windll.kernel32.GetExitCodeProcess(pHandle, ctypes.byref(pExitCode))
    ctypes.windll.kernel32.CloseHandle(pHandle)
    return pExitCode.value == STILL_ACTIVE

else:
  import errno

  def isPidAlive(pid):
    try:
      # kill(pid, 0) checks for a valid PID without actually sending a signal
      # The method throws OSError if the PID is invalid, which we catch below.
      os.kill(pid, 0)

      # Wait on it to see if it's a zombie. This can throw OSError.ECHILD if
      # the process terminates before we get to this point.
      wpid, wstatus = os.waitpid(pid, os.WNOHANG)
      return wpid == 0
    except OSError, err:
      # Catch the errors we might expect from os.kill/os.waitpid,
      # and re-raise any others
      if err.errno == errno.ESRCH or err.errno == errno.ECHILD:
        return False
      raise
# TODO: ^ upstream isPidAlive to mozprocess

#######################
# HTTP SERVER SUPPORT #
#######################

class MochitestServer(object):
  "Web server used to serve Mochitests, for closer fidelity to the real web."

  def __init__(self, options):
    if isinstance(options, optparse.Values):
      options = vars(options)
    self._closeWhenDone = options['closeWhenDone']
    self._utilityPath = options['utilityPath']
    self._xrePath = options['xrePath']
    self._profileDir = options['profilePath']
    self.webServer = options['webServer']
    self.httpPort = options['httpPort']
    self.shutdownURL = "http://%(server)s:%(port)s/server/shutdown" % { "server" : self.webServer, "port" : self.httpPort }
    self.testPrefix = "'webapprt_'" if options.get('webapprtContent') else "undefined"

    if options.get('httpdPath'):
        self._httpdPath = options['httpdPath']
    else:
        self._httpdPath = '.'
    self._httpdPath = os.path.abspath(self._httpdPath)

  def start(self):
    "Run the Mochitest server, returning the process ID of the server."

    # get testing environment
    env = environment(xrePath=self._xrePath)
    env["XPCOM_DEBUG_BREAK"] = "warn"

    # When running with an ASan build, our xpcshell server will also be ASan-enabled,
    # thus consuming too much resources when running together with the browser on
    # the test slaves. Try to limit the amount of resources by disabling certain
    # features.
    env["ASAN_OPTIONS"] = "quarantine_size=1:redzone=32"

    if mozinfo.isWin:
      env["PATH"] = env["PATH"] + ";" + str(self._xrePath)

    args = ["-g", self._xrePath,
            "-v", "170",
            "-f", self._httpdPath + "/httpd.js",
            "-e", """const _PROFILE_PATH = '%(profile)s'; const _SERVER_PORT = '%(port)s'; const _SERVER_ADDR = '%(server)s'; const _TEST_PREFIX = %(testPrefix)s; const _DISPLAY_RESULTS = %(displayResults)s;""" %
                   {"profile" : self._profileDir.replace('\\', '\\\\'), "port" : self.httpPort, "server" : self.webServer,
                    "testPrefix" : self.testPrefix, "displayResults" : str(not self._closeWhenDone).lower() },
            "-f", "./" + "server.js"]

    xpcshell = os.path.join(self._utilityPath,
                            "xpcshell" + mozinfo.info['bin_suffix'])
    command = [xpcshell] + args
    self._process = mozprocess.ProcessHandler(command, env=env)
    self._process.run()
    log.info("%s : launching %s", self.__class__.__name__, command)
    pid = self._process.pid
    log.info("runtests.py | Server pid: %d", pid)

  def ensureReady(self, timeout):
    assert timeout >= 0

    aliveFile = os.path.join(self._profileDir, "server_alive.txt")
    i = 0
    while i < timeout:
      if os.path.exists(aliveFile):
        break
      time.sleep(1)
      i += 1
    else:
      log.error("TEST-UNEXPECTED-FAIL | runtests.py | Timed out while waiting for server startup.")
      self.stop()
      sys.exit(1)

  def stop(self):
    try:
      with urllib2.urlopen(self.shutdownURL) as c:
        c.read()

      # TODO: need ProcessHandler.poll()
      # https://bugzilla.mozilla.org/show_bug.cgi?id=912285
      #      rtncode = self._process.poll()
      rtncode = self._process.proc.poll()
      if rtncode is None:
        # TODO: need ProcessHandler.terminate() and/or .send_signal()
        # https://bugzilla.mozilla.org/show_bug.cgi?id=912285
        # self._process.terminate()
        self._process.proc.terminate()
    except:
      self._process.kill()

class WebSocketServer(object):
  "Class which encapsulates the mod_pywebsocket server"

  def __init__(self, options, scriptdir, debuggerInfo=None):
    self.port = options.webSocketPort
    self._scriptdir = scriptdir
    self.debuggerInfo = debuggerInfo

  def start(self):
    # Invoke pywebsocket through a wrapper which adds special SIGINT handling.
    #
    # If we're in an interactive debugger, the wrapper causes the server to
    # ignore SIGINT so the server doesn't capture a ctrl+c meant for the
    # debugger.
    #
    # If we're not in an interactive debugger, the wrapper causes the server to
    # die silently upon receiving a SIGINT.
    scriptPath = 'pywebsocket_wrapper.py'
    script = os.path.join(self._scriptdir, scriptPath)

    cmd = [sys.executable, script]
    if self.debuggerInfo and self.debuggerInfo['interactive']:
        cmd += ['--interactive']
    cmd += ['-p', str(self.port), '-w', self._scriptdir, '-l',      \
           os.path.join(self._scriptdir, "websock.log"),            \
           '--log-level=debug', '--allow-handlers-outside-root-dir']
    # start the process
    self._process = mozprocess.ProcessHandler(cmd)
    self._process.run()
    pid = self._process.pid
    log.info("runtests.py | Websocket server pid: %d", pid)

  def stop(self):
    self._process.kill()

class MochitestUtilsMixin(object):
  """
  Class containing some utility functions common to both local and remote
  mochitest runners
  """

  # TODO Utility classes are a code smell. This class is temporary
  #      and should be removed when desktop mochitests are refactored
  #      on top of mozbase. Each of the functions in here should
  #      probably live somewhere in mozbase

  oldcwd = os.getcwd()
  jarDir = 'mochijar'

  # Path to the test script on the server
  TEST_PATH = "tests"
  CHROME_PATH = "redirect.html"
  urlOpts = []

  def __init__(self):
    os.chdir(SCRIPT_DIR)
    self.update_mozinfo()

  def update_mozinfo(self):
    """walk up directories to find mozinfo.json update the info"""
    # TODO: This should go in a more generic place, e.g. mozinfo

    path = SCRIPT_DIR
    dirs = set()
    while path != os.path.expanduser('~'):
        if path in dirs:
            break
        dirs.add(path)
        path = os.path.split(path)[0]

    mozinfo.find_and_update_from_json(*dirs)

  def getFullPath(self, path):
    " Get an absolute path relative to self.oldcwd."
    return os.path.normpath(os.path.join(self.oldcwd, os.path.expanduser(path)))

  def getLogFilePath(self, logFile):
    """ return the log file path relative to the device we are testing on, in most cases
        it will be the full path on the local system
    """
    return self.getFullPath(logFile)

  def buildURLOptions(self, options, env):
    """ Add test control options from the command line to the url

        URL parameters to test URL:

        autorun -- kick off tests automatically
        closeWhenDone -- closes the browser after the tests
        hideResultsTable -- hides the table of individual test results
        logFile -- logs test run to an absolute path
        totalChunks -- how many chunks to split tests into
        thisChunk -- which chunk to run
        timeout -- per-test timeout in seconds
        repeat -- How many times to repeat the test, ie: repeat=1 will run the test twice.
    """

    # allow relative paths for logFile
    if options.logFile:
      options.logFile = self.getLogFilePath(options.logFile)
    if options.browserChrome or options.chrome or options.a11y or options.webapprtChrome:
      self.makeTestConfig(options)
    else:
      if options.autorun:
        self.urlOpts.append("autorun=1")
      if options.timeout:
        self.urlOpts.append("timeout=%d" % options.timeout)
      if options.closeWhenDone:
        self.urlOpts.append("closeWhenDone=1")
      if options.logFile:
        self.urlOpts.append("logFile=" + encodeURIComponent(options.logFile))
        self.urlOpts.append("fileLevel=" + encodeURIComponent(options.fileLevel))
      if options.consoleLevel:
        self.urlOpts.append("consoleLevel=" + encodeURIComponent(options.consoleLevel))
      if options.totalChunks:
        self.urlOpts.append("totalChunks=%d" % options.totalChunks)
        self.urlOpts.append("thisChunk=%d" % options.thisChunk)
      if options.chunkByDir:
        self.urlOpts.append("chunkByDir=%d" % options.chunkByDir)
      if options.shuffle:
        self.urlOpts.append("shuffle=1")
      if "MOZ_HIDE_RESULTS_TABLE" in env and env["MOZ_HIDE_RESULTS_TABLE"] == "1":
        self.urlOpts.append("hideResultsTable=1")
      if options.runUntilFailure:
        self.urlOpts.append("runUntilFailure=1")
      if options.repeat:
        self.urlOpts.append("repeat=%d" % options.repeat)
      if os.path.isfile(os.path.join(self.oldcwd, os.path.dirname(__file__), self.TEST_PATH, options.testPath)) and options.repeat > 0:
        self.urlOpts.append("testname=%s" % ("/").join([self.TEST_PATH, options.testPath]))
      if options.testManifest:
        self.urlOpts.append("testManifest=%s" % options.testManifest)
        if hasattr(options, 'runOnly') and options.runOnly:
          self.urlOpts.append("runOnly=true")
        else:
          self.urlOpts.append("runOnly=false")
      if options.manifestFile:
        self.urlOpts.append("manifestFile=%s" % options.manifestFile)
      if options.failureFile:
        self.urlOpts.append("failureFile=%s" % self.getFullPath(options.failureFile))
      if options.runSlower:
        self.urlOpts.append("runSlower=true")

  def buildTestPath(self, options):
    """ Build the url path to the specific test harness and test file or directory
        Build a manifest of tests to run and write out a json file for the harness to read
    """
    if options.manifestFile and os.path.isfile(options.manifestFile):
      manifest = TestManifest(strict=False)
      manifest.read(options.manifestFile)
      # Bug 883858 - return all tests including disabled tests
      tests = manifest.active_tests(disabled=False, **mozinfo.info)
      paths = []
      for test in tests:
        tp = test['path'].split(self.getTestRoot(options), 1)[1].strip('/')

        # Filter out tests if we are using --test-path
        if options.testPath and not tp.startswith(options.testPath):
          continue

        paths.append({'path': tp})

      # Bug 883865 - add this functionality into manifestDestiny
      with open('tests.json', 'w') as manifestFile:
        manifestFile.write(json.dumps({'tests': paths}))
      options.manifestFile = 'tests.json'

    testHost = "http://mochi.test:8888"
    testURL = ("/").join([testHost, self.TEST_PATH, options.testPath])
    if os.path.isfile(os.path.join(self.oldcwd, os.path.dirname(__file__), self.TEST_PATH, options.testPath)) and options.repeat > 0:
       testURL = ("/").join([testHost, self.PLAIN_LOOP_PATH])
    if options.chrome or options.a11y:
       testURL = ("/").join([testHost, self.CHROME_PATH])
    elif options.browserChrome:
      testURL = "about:blank"
    elif options.ipcplugins:
      testURL = ("/").join([testHost, self.TEST_PATH, "dom/plugins/test"])
    return testURL

  def startWebSocketServer(self, options, debuggerInfo):
    """ Launch the websocket server """
    if options.webServer != '127.0.0.1':
      return

    self.wsserver = WebSocketServer(options, SCRIPT_DIR, debuggerInfo)
    self.wsserver.start()

  def stopWebSocketServer(self, options):
    if options.webServer != '127.0.0.1':
      return

    self.wsserver.stop()

  def startWebServer(self, options):
    """Create the webserver and start it up"""

    if options.webServer != '127.0.0.1':
      return

    self.server = MochitestServer(options)
    self.server.start()

    # If we're lucky, the server has fully started by now, and all paths are
    # ready, etc.  However, xpcshell cold start times suck, at least for debug
    # builds.  We'll try to connect to the server for awhile, and if we fail,
    # we'll try to kill the server and exit with an error.
    self.server.ensureReady(self.SERVER_STARTUP_TIMEOUT)

  def stopWebServer(self, options):
    """ Server's no longer needed, and perhaps more importantly, anything it might
        spew to console shouldn't disrupt the leak information table we print next.
    """
    if options.webServer != '127.0.0.1':
      return

    self.server.stop()

  def copyExtraFilesToProfile(self, options):
    "Copy extra files or dirs specified on the command line to the testing profile."
    for f in options.extraProfileFiles:
      abspath = self.getFullPath(f)
      if os.path.isfile(abspath):
        shutil.copy2(abspath, options.profilePath)
      elif os.path.isdir(abspath):
        dest = os.path.join(options.profilePath, os.path.basename(abspath))
        shutil.copytree(abspath, dest)
      else:
        log.warning("runtests.py | Failed to copy %s to profile", abspath)

  def copyTestsJarToProfile(self, options):
    """ copy tests.jar to the profile directory so we can auto register it in the .xul harness """
    testsJarFile = os.path.join(SCRIPT_DIR, "tests.jar")
    if not os.path.isfile(testsJarFile):
      return False

    shutil.copy2(testsJarFile, options.profilePath)
    return True

  def installChromeJar(self, chrome, options):
    """
      copy mochijar directory to profile as an extension so we have chrome://mochikit for all harness code
    """
    # Write chrome.manifest.
    with open(os.path.join(options.profilePath, "extensions", "staged", "mochikit@mozilla.org", "chrome.manifest"), "a") as mfile:
      mfile.write(chrome)

  def addChromeToProfile(self, options):
    "Adds MochiKit chrome tests to the profile."

    # Create (empty) chrome directory.
    chromedir = os.path.join(options.profilePath, "chrome")
    os.mkdir(chromedir)

    # Write userChrome.css.
    chrome = """
@namespace url("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"); /* set default namespace to XUL */
toolbar,
toolbarpalette {
  background-color: rgb(235, 235, 235) !important;
}
toolbar#nav-bar {
  background-image: none !important;
}
"""
    with open(os.path.join(options.profilePath, "userChrome.css"), "a") as chromeFile:
      chromeFile.write(chrome)

    # Call copyTestsJarToProfile(), Write tests.manifest.
    manifest = os.path.join(options.profilePath, "tests.manifest")
    with open(manifest, "w") as manifestFile:
      if self.copyTestsJarToProfile(options):
        # Register tests.jar.
        manifestFile.write("content mochitests jar:tests.jar!/content/\n");
      else:
        # Register chrome directory.
        chrometestDir = os.path.abspath(".") + "/"
        if mozinfo.isWin:
          chrometestDir = "file:///" + chrometestDir.replace("\\", "/")
        manifestFile.write("content mochitests %s contentaccessible=yes\n" % chrometestDir)

      if options.testingModulesDir is not None:
        manifestFile.write("resource testing-common file:///%s\n" %
          options.testingModulesDir)

    # Call installChromeJar().
    if not os.path.isdir(os.path.join(SCRIPT_DIR, self.jarDir)):
      log.testFail("invalid setup: missing mochikit extension")
      return None

    # Support Firefox (browser), B2G (shell), SeaMonkey (navigator), and Webapp
    # Runtime (webapp).
    chrome = ""
    if options.browserChrome or options.chrome or options.a11y or options.webapprtChrome:
      chrome += """
overlay chrome://browser/content/browser.xul chrome://mochikit/content/browser-test-overlay.xul
overlay chrome://browser/content/shell.xhtml chrome://mochikit/content/browser-test-overlay.xul
overlay chrome://navigator/content/navigator.xul chrome://mochikit/content/browser-test-overlay.xul
overlay chrome://webapprt/content/webapp.xul chrome://mochikit/content/browser-test-overlay.xul
"""

    self.installChromeJar(chrome, options)
    return manifest

  def getExtensionsToInstall(self, options):
    "Return a list of extensions to install in the profile"
    extensions = options.extensionsToInstall or []
    appDir = options.app[:options.app.rfind(os.sep)] if options.app else options.utilityPath

    extensionDirs = [
      # Extensions distributed with the test harness.
      os.path.normpath(os.path.join(SCRIPT_DIR, "extensions")),
    ]
    if appDir:
      # Extensions distributed with the application.
      extensionDirs.append(os.path.join(appDir, "distribution", "extensions"))

    for extensionDir in extensionDirs:
      if os.path.isdir(extensionDir):
        for dirEntry in os.listdir(extensionDir):
          if dirEntry not in options.extensionsToExclude:
            path = os.path.join(extensionDir, dirEntry)
            if os.path.isdir(path) or (os.path.isfile(path) and path.endswith(".xpi")):
              extensions.append(path)

    # append mochikit
    extensions.append(os.path.join(SCRIPT_DIR, self.jarDir))
    return extensions


class Mochitest(MochitestUtilsMixin):
  runSSLTunnel = True
  vmwareHelper = None
  DEFAULT_TIMEOUT = 60.0

  # XXX use automation.py for test name to avoid breaking legacy
  # TODO: replace this with 'runtests.py' or 'mochitest' or the like
  test_name = 'automation.py'

  def __init__(self):
    super(Mochitest, self).__init__()

    # environment function for browserEnv
    self.environment = environment

    # Max time in seconds to wait for server startup before tests will fail -- if
    # this seems big, it's mostly for debug machines where cold startup
    # (particularly after a build) takes forever.
    self.SERVER_STARTUP_TIMEOUT = 180 if mozinfo.info.get('debug') else 90

    # metro browser sub process id
    self.browserProcessId = None

    # cached server locations
    self._locations = {}

    self.haveDumpedScreen = False

  def extraPrefs(self, extraPrefs):
    """interpolate extra preferences from option strings"""

    try:
      return dict(parseKeyValue(extraPrefs, context='--setpref='))
    except KeyValueParseError, e:
      print str(e)
      sys.exit(1)

  def buildProfile(self, options):
    """ create the profile and add optional chrome bits and files if requested """
    if options.browserChrome and options.timeout:
      options.extraPrefs.append("testing.browserTestHarness.timeout=%d" % options.timeout)

    # get extensions to install
    extensions = self.getExtensionsToInstall(options)

    # web apps
    appsPath = os.path.join(SCRIPT_DIR, 'profile_data', 'webapps_mochitest.json')
    if os.path.exists(appsPath):
      with open(appsPath) as apps_file:
        apps = json.load(apps_file)
    else:
      apps = None

    # locations
    locations_file = os.path.join(SCRIPT_DIR, 'server-locations.txt')
    locations = ServerLocations(locations_file)

    # preferences
    prefsPath = os.path.join(SCRIPT_DIR, 'profile_data', 'prefs_general.js')
    prefs = dict(Preferences.read_prefs(prefsPath))
    prefs.update(self.extraPrefs(options.extraPrefs))

    # interpolate preferences
    interpolation = {"server": "%s:%s" % (options.webServer, options.httpPort)}
    prefs = json.loads(json.dumps(prefs) % interpolation)
    for pref in prefs:
      prefs[pref] = Preferences.cast(prefs[pref])
    # TODO: make this less hacky
    # https://bugzilla.mozilla.org/show_bug.cgi?id=913152

    # proxy
    proxy = {'remote': options.webServer,
             'http': options.httpPort,
             'https': options.sslPort,
    # use SSL port for legacy compatibility; see
    # - https://bugzilla.mozilla.org/show_bug.cgi?id=688667#c66
    # - https://bugzilla.mozilla.org/show_bug.cgi?id=899221
    # - https://github.com/mozilla/mozbase/commit/43f9510e3d58bfed32790c82a57edac5f928474d
    #             'ws': str(self.webSocketPort)
             'ws': options.sslPort
             }

    # create a profile
    self.profile = Profile(profile=options.profilePath,
                           addons=extensions,
                           locations=locations,
                           preferences=prefs,
                           apps=apps,
                           proxy=proxy
                           )

    # Fix options.profilePath for legacy consumers.
    options.profilePath = self.profile.profile

    manifest = self.addChromeToProfile(options)
    self.copyExtraFilesToProfile(options)
    return manifest

  def buildBrowserEnv(self, options):
    """build the environment variables for the specific test and operating system"""
    browserEnv = self.environment(xrePath=options.xrePath)

    # These variables are necessary for correct application startup; change
    # via the commandline at your own risk.
    browserEnv["XPCOM_DEBUG_BREAK"] = "stack"

    # interpolate environment passed with options
    try:
      browserEnv.update(dict(parseKeyValue(options.environment, context='--setenv')))
    except KeyValueParseError, e:
      log.error(str(e))
      return

    browserEnv["XPCOM_MEM_BLOAT_LOG"] = self.leak_report_file

    if options.fatalAssertions:
      browserEnv["XPCOM_DEBUG_BREAK"] = "stack-and-abort"

    return browserEnv

  def cleanup(self, manifest, options):
    """ remove temporary files and profile """
    os.remove(manifest)
    del self.profile

  def dumpScreen(self, utilityPath):
    # TODO: this code may be moved to automationutils.py
    # see also: https://bugzilla.mozilla.org/show_bug.cgi?id=749421

    if self.haveDumpedScreen:
      log.info("Not taking screenshot here: see the one that was previously logged")
      return

    self.haveDumpedScreen = True

    # Need to figure out what tool and whether it write to a file or stdout
    if mozinfo.isUnix:
      utility = [os.path.join(utilityPath, "screentopng")]
      imgoutput = 'stdout'
    elif mozinfo.isMac:
      utility = ['/usr/sbin/screencapture', '-C', '-x', '-t', 'png']
      imgoutput = 'file'
    elif mozinfo.isWin:
      utility = [os.path.join(utilityPath, "screenshot.exe")]
      imgoutput = 'file'

    # Run the capture correctly for the type of capture
    if imgoutput == 'file':
      tmpfd, imgfilename = tempfile.mkstemp(prefix='mozilla-test-fail_')
      os.close(tmpfd)
      utility.append(imgfilename)
      kwargs = {}
    elif imgoutput == 'stdout':
      kwargs=dict(bufsize=-1, close_fds=True)
    try:
      dumper = mozprocess.ProcessHandler(utility, **kwargs)
      dumper.run()
    except OSError, err:
      log.info("Failed to start %s for screenshot: %s",
               utility[0], err.strerror)
      return

    # Check whether the capture utility ran successfully
    returncode = dumper.wait()
    if returncode:
      log.info("%s exited with code %d", utility, returncode)
      return

    try:
      if imgoutput == 'stdout':
        image = '\n'.join(dumper.output)
      elif imgoutput == 'file':
        with open(imgfilename, 'rb') as imgfile:
          image = imgfile.read()
    except IOError, err:
        log.info("Failed to read image from %s", imgoutput)

    encoded = base64.b64encode(image)
    log.info("SCREENSHOT: data:image/png;base64,%s", encoded)

  def killAndGetStack(self, processPID, utilityPath, debuggerInfo, dump_screen=False):
    """
    Kill the process, preferrably in a way that gets us a stack trace.
    Also attempts to obtain a screenshot before killing the process
    if specified.
    """

    if dump_screen:
      self.dumpScreen(utilityPath)

    if mozinfo.info.get('crashreporter', True) and not debuggerInfo:
      if mozinfo.isWin:
        # We should have a "crashinject" program in our utility path
        crashinject = os.path.normpath(os.path.join(utilityPath, "crashinject.exe"))
        if os.path.exists(crashinject) and subprocess.Popen([crashinject, str(processPID)]).wait() == 0:
          return
      else:
        try:
          os.kill(processPID, signal.SIGABRT)
        except OSError:
          # https://bugzilla.mozilla.org/show_bug.cgi?id=921509
          log.info("Can't trigger Breakpad, process no longer exists")
        return
    log.info("Can't trigger Breakpad, just killing process")
    killPid(processPID)

  def checkForZombies(self, processLog, utilityPath, debuggerInfo):
    """Look for hung processes"""

    if not os.path.exists(processLog):
      log.info('Automation Error: PID log not found: %s', processLog)
      # Whilst no hung process was found, the run should still display as a failure
      return True

    # scan processLog for zombies
    log.info('INFO | zombiecheck | Reading PID log: %s', processLog)
    processList = []
    pidRE = re.compile(r'launched child process (\d+)$')
    with open(processLog) as processLogFD:
      for line in processLogFD:
        log.info(line.rstrip())
        m = pidRE.search(line)
        if m:
          processList.append(int(m.group(1)))

    # kill zombies
    foundZombie = False
    for processPID in processList:
      log.info("INFO | zombiecheck | Checking for orphan process with PID: %d", processPID)
      if isPidAlive(processPID):
        foundZombie = True
        log.info("TEST-UNEXPECTED-FAIL | zombiecheck | child process %d still alive after shutdown", processPID)
        self.killAndGetStack(processPID, utilityPath, debuggerInfo, dump_screen=not debuggerInfo)

    return foundZombie

  def startVMwareRecording(self, options):
    """ starts recording inside VMware VM using the recording helper dll """
    assert mozinfo.isWin
    from ctypes import cdll
    self.vmwareHelper = cdll.LoadLibrary(self.vmwareHelperPath)
    if self.vmwareHelper is None:
      log.warning("runtests.py | Failed to load "
                   "VMware recording helper")
      return
    log.info("runtests.py | Starting VMware recording.")
    try:
      self.vmwareHelper.StartRecording()
    except Exception, e:
      log.warning("runtests.py | Failed to start "
                  "VMware recording: (%s)" % str(e))
      self.vmwareHelper = None

  def stopVMwareRecording(self):
    """ stops recording inside VMware VM using the recording helper dll """
    assert mozinfo.isWin
    if self.vmwareHelper is not None:
      log.info("runtests.py | Stopping VMware recording.")
      try:
        self.vmwareHelper.StopRecording()
      except Exception, e:
        log.warning("runtests.py | Failed to stop "
                    "VMware recording: (%s)" % str(e))
      self.vmwareHelper = None

  def runApp(self,
             testUrl,
             env,
             app,
             profile,
             extraArgs,
             utilityPath,
             xrePath,
             certPath,
             debuggerInfo=None,
             symbolsPath=None,
             timeout=-1,
             onLaunch=None):
    """
    Run the app, log the duration it took to execute, return the status code.
    Kills the app if it runs for longer than |maxTime| seconds, or outputs nothing for |timeout| seconds.
    """

    # debugger information
    interactive = False
    debug_args = None
    if debuggerInfo:
        interactive = debuggerInfo['interactive']
        debug_args = [debuggerInfo['path']] + debuggerInfo['args']

    # ensure existence of required paths
    required_paths = ('utilityPath', 'xrePath', 'certPath')
    missing = [(path, locals()[path])
               for path in required_paths
               if not os.path.exists(locals()[path])]
    if missing:
      log.error("runtests.py | runApp called with missing paths: %s" % (
        ', '.join([("%s->%s" % (key, value)) for key, value in missing])))
      return 1

    # fix default timeout
    if timeout == -1:
      timeout = self.DEFAULT_TIMEOUT

    # build parameters
    is_test_build = mozinfo.info.get('tests_enabled', True)
    bin_suffix = mozinfo.info.get('bin_suffix', '')

    # copy env so we don't munge the caller's environment
    env = env.copy()

    # set process log environment variable
    tmpfd, processLog = tempfile.mkstemp(suffix='pidlog')
    os.close(tmpfd)
    env["MOZ_PROCESS_LOG"] = processLog

    if self.runSSLTunnel:

      # create certificate database for the profile
      # TODO: this should really be upstreamed somewhere, maybe mozprofile
      certificateStatus = self.fillCertificateDB(self.profile.profile,
                                                 certPath,
                                                 utilityPath,
                                                 xrePath)
      if certificateStatus:
        log.info("TEST-UNEXPECTED-FAIL | runtests.py | Certificate integration failed")
        return certificateStatus

      # start ssltunnel to provide https:// URLs capability
      ssltunnel = os.path.join(utilityPath, "ssltunnel" + bin_suffix)
      ssltunnel_cfg = os.path.join(self.profile.profile, "ssltunnel.cfg")
      ssltunnelProcess = mozprocess.ProcessHandler([ssltunnel, ssltunnel_cfg],
                                                    env=environment(xrePath=xrePath))
      ssltunnelProcess.run()
      log.info("INFO | runtests.py | SSL tunnel pid: %d", ssltunnelProcess.pid)
    else:
      ssltunnelProcess = None

    if interactive:
      # If an interactive debugger is attached,
      # don't use timeouts, and don't capture ctrl-c.
      timeout = None
      signal.signal(signal.SIGINT, lambda sigid, frame: None)

    # build command line
    cmd = os.path.abspath(app)
    args = list(extraArgs)
    # TODO: mozrunner should use -foreground at least for mac
    # https://bugzilla.mozilla.org/show_bug.cgi?id=916512
    args.append('-foreground')
    if testUrl:
       args.append(testUrl)

    # create an instance to process the output
    outputHandler = self.OutputHandler(harness=self,
                                       utilityPath=utilityPath,
                                       symbolsPath=symbolsPath,
                                       dump_screen_on_timeout=not debuggerInfo,
      )

    # if the output handler is a pipe, it will process output via the subprocess
    kp_kwargs = {} if outputHandler.pipe else {'processOutputLine': [outputHandler]}

    # create mozrunner instance and start the system under test process
    self.lastTestSeen = self.test_name
    startTime = datetime.now()

    # b2g desktop requires FirefoxRunner even though appname is b2g
    if mozinfo.info.get('appname') == 'b2g' and mozinfo.info.get('toolkit') != 'gonk':
        runner_cls = mozrunner.FirefoxRunner
    else:
        runner_cls = mozrunner.runners.get(mozinfo.info.get('appname', 'firefox'),
                                           mozrunner.Runner)
    runner = runner_cls(profile=self.profile,
                        binary=cmd,
                        cmdargs=args,
                        env=env,
                        process_class=mozprocess.ProcessHandlerMixin,
                        kp_kwargs=kp_kwargs,
                        )

    # XXX work around bug 898379 until mozrunner is updated for m-c; see
    # https://bugzilla.mozilla.org/show_bug.cgi?id=746243#c49
    runner.kp_kwargs = kp_kwargs

    # start the runner
    runner.start(debug_args=debug_args,
                 interactive=interactive,
                 outputTimeout=timeout)
    proc = runner.process_handler
    log.info("INFO | runtests.py | Application pid: %d", proc.pid)

    # set process information on the output handler
    outputHandler.setProcess(proc, timeout)

    if onLaunch is not None:
      # Allow callers to specify an onLaunch callback to be fired after the
      # app is launched.
      # We call onLaunch for b2g desktop mochitests so that we can
      # run a Marionette script after gecko has completed startup.
      onLaunch()

    # wait until app is finished
    # XXX copy functionality from
    # https://github.com/mozilla/mozbase/blob/master/mozrunner/mozrunner/runner.py#L61
    # until bug 913970 is fixed regarding mozrunner `wait` not returning status
    # see https://bugzilla.mozilla.org/show_bug.cgi?id=913970
    status = proc.wait()
    runner.process_handler = None

    # finalize output handler
    outputHandler.finish(proc.didTimeout)

    # handle timeout
    if proc.didTimeout:
      browserProcessId = outputHandler.browserProcessId
      self.handleTimeout(timeout, proc, utilityPath, debuggerInfo, browserProcessId)

    # record post-test information
    if not status:
      self.lastTestSeen = 'Main app process exited normally'
    log.info("INFO | runtests.py | exit %s", status)
    log.info("INFO | runtests.py | Application ran for: %s", str(datetime.now() - startTime))

    # Do a final check for zombie child processes.
    zombieProcesses = self.checkForZombies(processLog, utilityPath, debuggerInfo)

    # check for crashes
    minidump_path = os.path.join(self.profile.profile, "minidumps")
    crashed = mozcrash.check_for_crashes(minidump_path,
                                         symbolsPath,
                                         test_name=self.lastTestSeen)

    if crashed or zombieProcesses:
      status = 1

    # cleanup
    if os.path.exists(processLog):
      os.remove(processLog)
    if ssltunnelProcess:
      ssltunnelProcess.kill()

    return status

  def runTests(self, options, onLaunch=None):
    """ Prepare, configure, run tests and cleanup """

    # get debugger info, a dict of:
    # {'path': path to the debugger (string),
    #  'interactive': whether the debugger is interactive or not (bool)
    #  'args': arguments to the debugger (list)
    # TODO: use mozrunner.local.debugger_arguments:
    # https://github.com/mozilla/mozbase/blob/master/mozrunner/mozrunner/local.py#L42
    debuggerInfo = getDebuggerInfo(self.oldcwd,
                                   options.debugger,
                                   options.debuggerArgs,
                                   options.debuggerInteractive)

    self.leak_report_file = os.path.join(options.profilePath, "runtests_leaks.log")

    browserEnv = self.buildBrowserEnv(options)
    if browserEnv is None:
      return 1

    # buildProfile sets self.profile .
    # This relies on sideeffects and isn't very stateful:
    # https://bugzilla.mozilla.org/show_bug.cgi?id=919300
    manifest = self.buildProfile(options)
    if manifest is None:
      return 1

    # start servers and set ports
    # TODO: pass these values, don't set on `self`
    self.webServer = options.webServer
    self.httpPort = options.httpPort
    self.sslPort = options.sslPort
    self.webSocketPort = options.webSocketPort
    self.startWebServer(options)
    self.startWebSocketServer(options, debuggerInfo)

    testURL = self.buildTestPath(options)
    self.buildURLOptions(options, browserEnv)
    if self.urlOpts:
      testURL += "?" + "&".join(self.urlOpts)

    if options.webapprtContent:
      options.browserArgs.extend(('-test-mode', testURL))
      testURL = None

    if options.immersiveMode:
      options.browserArgs.extend(('-firefoxpath', options.app))
      options.app = self.immersiveHelperPath

    # Remove the leak detection file so it can't "leak" to the tests run.
    # The file is not there if leak logging was not enabled in the application build.
    if os.path.exists(self.leak_report_file):
      os.remove(self.leak_report_file)

    # then again to actually run mochitest
    if options.timeout:
      timeout = options.timeout + 30
    elif options.debugger or not options.autorun:
      timeout = None
    else:
      timeout = 330.0 # default JS harness timeout is 300 seconds

    if options.vmwareRecording:
      self.startVMwareRecording(options);

    log.info("runtests.py | Running tests: start.\n")
    try:
      status = self.runApp(testURL,
                           browserEnv,
                           options.app,
                           profile=self.profile,
                           extraArgs=options.browserArgs,
                           utilityPath=options.utilityPath,
                           xrePath=options.xrePath,
                           certPath=options.certPath,
                           debuggerInfo=debuggerInfo,
                           symbolsPath=options.symbolsPath,
                           timeout=timeout,
                           onLaunch=onLaunch
                           )
    except KeyboardInterrupt:
      log.info("runtests.py | Received keyboard interrupt.\n");
      status = -1
    except:
      traceback.print_exc()
      log.error("Automation Error: Received unexpected exception while running application\n")
      status = 1

    if options.vmwareRecording:
      self.stopVMwareRecording();

    self.stopWebServer(options)
    self.stopWebSocketServer(options)
    processLeakLog(self.leak_report_file, options.leakThreshold)

    log.info("runtests.py | Running tests: end.")

    if manifest is not None:
      self.cleanup(manifest, options)
    return status

  def handleTimeout(self, timeout, proc, utilityPath, debuggerInfo, browserProcessId):
    """handle process output timeout"""
    # TODO: bug 913975 : _processOutput should call self.processOutputLine one more time one timeout (I think)
    log.info("TEST-UNEXPECTED-FAIL | %s | application timed out after %d seconds with no output", self.lastTestSeen, int(timeout))
    browserProcessId = browserProcessId or proc.pid
    self.killAndGetStack(browserProcessId, utilityPath, debuggerInfo, dump_screen=not debuggerInfo)

  ### output processing

  class OutputHandler(object):
    """line output handler for mozrunner"""
    def __init__(self, harness, utilityPath, symbolsPath=None, dump_screen_on_timeout=True):
      """
      harness -- harness instance
      dump_screen_on_timeout -- whether to dump the screen on timeout
      """
      self.harness = harness
      self.utilityPath = utilityPath
      self.symbolsPath = symbolsPath
      self.dump_screen_on_timeout = dump_screen_on_timeout

      # perl binary to use
      self.perl = which('perl')

      # With metro browser runs this script launches the metro test harness which launches the browser.
      # The metro test harness hands back the real browser process id via log output which we need to
      # pick up on and parse out. This variable tracks the real browser process id if we find it.
      self.browserProcessId = None

      # stack fixer function and/or process
      self.stackFixerFunction, self.stackFixerCommand = self.stackFixer()
      self.stackFixerProcess = None

      # if there is a stack fixer subprocess, function as a pipe
      self.pipe = bool(self.stackFixerCommand)

    def processOutputLine(self, line):
      """per line handler of output for mozprocess"""
      for handler in self.outputHandlers():
        line = handler(line)
    __call__ = processOutputLine

    def outputHandlers(self):
      """returns ordered list of output handlers"""
      return [self.fix_stack,
              self.format,
              self.record_last_test,
              self.dumpScreenOnTimeout,
              self.metro_subprocess_id,
              self.log,
              ]

    def stackFixer(self):
      """
      return 2-tuple, (stackFixerFunction, StackFixerProcess),
      if any, to use on the output lines
      """

      if not mozinfo.info.get('debug'):
        return None, None

      stackFixerFunction = stackFixerCommand = None

      def import_stackFixerModule(module_name):
        sys.path.insert(0, self.utilityPath)
        module = __import__(module_name, globals(), locals(), [])
        sys.path.pop(0)
        return module

      if self.symbolsPath and os.path.exists(self.symbolsPath):
        # Run each line through a function in fix_stack_using_bpsyms.py (uses breakpad symbol files)
        # This method is preferred for Tinderbox builds, since native symbols may have been stripped.
        stackFixerModule = import_stackFixerModule('fix_stack_using_bpsyms')
        stackFixerFunction = lambda line: stackFixerModule.fixSymbols(line, self.symbolsPath)

      elif mozinfo.isLinux and self.perl:
        # Run logsource through fix-linux-stack.pl (uses addr2line)
        # This method is preferred for developer machines, so we don't have to run "make buildsymbols".

        stackFixerCommand = [self.perl, os.path.join(self.utilityPath, "fix-linux-stack.pl")]

      return (stackFixerFunction, stackFixerCommand)

    def setProcess(self, proc, outputTimeout=None):
      self.proc = proc
      if self.stackFixerCommand:
        self.stackFixerProcess = mozprocess.ProcessHandler(self.stackFixerCommand,
                                                           stdin=proc.proc.stdout,
                                                           processOutputLine=[self],
          )
        self.stackFixerProcess.run(outputTimeout=outputTimeout)

    def finish(self, didTimeout):
      if self.stackFixerProcess:
        status = self.stackFixerProcess.wait()
        if status and not didTimeout:
          log.info("TEST-UNEXPECTED-FAIL | runtests.py | Stack fixer process exited with code %d during test run", status)


    # output line handlers:
    # these take a line and return a line

    def fix_stack(self, line):
      if self.stackFixerFunction:
        return self.stackFixerFunction(line)
      return line

    def format(self, line):
      """format the line"""
      return line.rstrip().decode("UTF-8", "ignore")

    def record_last_test(self, line):
      """record last test on harness"""
      if "TEST-START" in line and "|" in line:
        self.harness.lastTestSeen = line.split("|")[1].strip()
      return line

    def dumpScreenOnTimeout(self, line):
      if self.dump_screen_on_timeout and "TEST-UNEXPECTED-FAIL" in line and "Test timed out" in line:
        self.harness.dumpScreen(self.utilityPath)
      return line

    def metro_subprocess_id(self, line):
      """look for metro browser subprocess id"""
      if "METRO_BROWSER_PROCESS" in line:
        index = line.find("=")
        if index != -1:
          self.browserProcessId = line[index+1:].rstrip()
          log.info("INFO | runtests.py | metro browser sub process id detected: %s", self.browserProcessId)
      return line

    def log(self, line):
      log.info(line)
      return line


  def makeTestConfig(self, options):
    "Creates a test configuration file for customizing test execution."
    def jsonString(val):
      if isinstance(val, bool):
        if val:
          return "true"
        return "false"
      elif val is None:
        return '""'
      elif isinstance(val, basestring):
        return '"%s"' % (val.replace('\\', '\\\\'))
      elif isinstance(val, int):
        return '%s' % (val)
      elif isinstance(val, list):
        content = '['
        first = True
        for item in val:
          if first:
            first = False
          else:
            content += ", "
          content += jsonString(item)
        content += ']'
        return content
      else:
        print "unknown type: %s: %s" % (opt, val)
        sys.exit(1)

    options.logFile = options.logFile.replace("\\", "\\\\")
    options.testPath = options.testPath.replace("\\", "\\\\")
    testRoot = self.getTestRoot(options)

    if "MOZ_HIDE_RESULTS_TABLE" in os.environ and os.environ["MOZ_HIDE_RESULTS_TABLE"] == "1":
      options.hideResultsTable = True

    #TODO: when we upgrade to python 2.6, just use json.dumps(options.__dict__)
    content = "{"
    content += '"testRoot": "%s", ' % (testRoot)
    first = True
    for opt in options.__dict__.keys():
      val = options.__dict__[opt]
      if first:
        first = False
      else:
        content += ", "

      content += '"' + opt + '": '
      content += jsonString(val)
    content += "}"

    with open(os.path.join(options.profilePath, "testConfig.js"), "w") as config:
      config.write(content)

  def getTestRoot(self, options):
    if (options.browserChrome):
      if (options.immersiveMode):
        return 'metro'
      return 'browser'
    elif (options.a11y):
      return 'a11y'
    elif (options.webapprtChrome):
      return 'webapprtChrome'
    elif (options.chrome):
      return 'chrome'
    return self.TEST_PATH

  def installExtensionFromPath(self, options, path, extensionID = None):
    """install an extension to options.profilePath"""

    # TODO: currently extensionID is unused; see
    # https://bugzilla.mozilla.org/show_bug.cgi?id=914267
    # [mozprofile] make extensionID a parameter to install_from_path
    # https://github.com/mozilla/mozbase/blob/master/mozprofile/mozprofile/addons.py#L169

    extensionPath = self.getFullPath(path)

    log.info("runtests.py | Installing extension at %s to %s." %
                (extensionPath, options.profilePath))

    addons = AddonManager(options.profilePath)

    # XXX: del the __del__
    # hack can be removed when mozprofile is mirrored to m-c ; see
    # https://bugzilla.mozilla.org/show_bug.cgi?id=911218 :
    # [mozprofile] AddonManager should only cleanup on __del__ optionally:
    # https://github.com/mozilla/mozbase/blob/master/mozprofile/mozprofile/addons.py#L266
    if hasattr(addons, '__del__'):
      del addons.__del__

    addons.install_from_path(path)

  def installExtensionsToProfile(self, options):
    "Install special testing extensions, application distributed extensions, and specified on the command line ones to testing profile."
    for path in self.getExtensionsToInstall(options):
      self.installExtensionFromPath(options, path)

  def readLocations(self, locations_file):
    """
    Reads the locations at which the Mochitest HTTP server is available from
    `locations_file`.
    """
    path = os.path.realpath(locations_file)
    return self._locations.setdefault(path, ServerLocations(path))

  def fillCertificateDB(self, profileDir, certPath, utilityPath, xrePath):
    # TODO: move -> mozprofile:
    # https://bugzilla.mozilla.org/show_bug.cgi?id=746243#c35

    pwfilePath = os.path.join(profileDir, ".crtdbpw")
    with open(pwfilePath, "w") as pwfile:
      pwfile.write("\n")

    # Create head of the ssltunnel configuration file
    sslTunnelConfigPath = os.path.join(profileDir, "ssltunnel.cfg")
    sslTunnelConfig = open(sslTunnelConfigPath, "w")
    sslTunnelConfig.write("httpproxy:1\n")
    sslTunnelConfig.write("certdbdir:%s\n" % certPath)
    sslTunnelConfig.write("forward:127.0.0.1:%s\n" % self.httpPort)
    sslTunnelConfig.write("websocketserver:%s:%s\n" % (self.webServer, self.webSocketPort))
    sslTunnelConfig.write("listen:*:%s:pgo server certificate\n" % self.sslPort)

    # Configure automatic certificate and bind custom certificates, client authentication
    locations = self.readLocations(os.path.join(SCRIPT_DIR, 'server-locations.txt'))

    for loc in locations:

      if loc.scheme == "https" and "nocert" not in loc.options:
        customCertRE = re.compile("^cert=(?P<nickname>[0-9a-zA-Z_ ]+)")
        clientAuthRE = re.compile("^clientauth=(?P<clientauth>[a-z]+)")
        redirRE      = re.compile("^redir=(?P<redirhost>[0-9a-zA-Z_ .]+)")
        for option in loc.options:
          match = customCertRE.match(option)
          if match:
            customcert = match.group("nickname");
            sslTunnelConfig.write("listen:%s:%s:%s:%s\n" %
                      (loc.host, loc.port, self.sslPort, customcert))

          match = clientAuthRE.match(option)
          if match:
            clientauth = match.group("clientauth");
            sslTunnelConfig.write("clientauth:%s:%s:%s:%s\n" %
                      (loc.host, loc.port, self.sslPort, clientauth))

          match = redirRE.match(option)
          if match:
            redirhost = match.group("redirhost")
            sslTunnelConfig.write("redirhost:%s:%s:%s:%s\n" %
                      (loc.host, loc.port, self.sslPort, redirhost))

    sslTunnelConfig.close()

    # Pre-create the certification database for the profile
    env = self.environment(xrePath=xrePath)
    bin_suffix = mozinfo.info.get('bin_suffix', '')
    certutil = os.path.join(utilityPath, "certutil" + bin_suffix)
    pk12util = os.path.join(utilityPath, "pk12util" + bin_suffix)

    status = call([certutil, "-N", "-d", profileDir, "-f", pwfilePath], env=env)
    if status:
      return status

    # Walk the cert directory and add custom CAs and client certs
    files = os.listdir(certPath)
    for item in files:
      root, ext = os.path.splitext(item)
      if ext == ".ca":
        trustBits = "CT,,"
        if root.endswith("-object"):
          trustBits = "CT,,CT"
        call([certutil, "-A", "-i", os.path.join(certPath, item),
              "-d", profileDir, "-f", pwfilePath, "-n", root, "-t", trustBits],
              env=env)
      elif ext == ".client":
        call([pk12util, "-i", os.path.join(certPath, item), "-w",
              pwfilePath, "-d", profileDir],
              env=env)

    os.unlink(pwfilePath)
    return 0


def main():

  # parse command line options
  mochitest = Mochitest()
  parser = MochitestOptions()
  options, args = parser.parse_args()
  options = parser.verifyOptions(options, mochitest)
  if options is None:
    # parsing error
    sys.exit(1)

  options.utilityPath = mochitest.getFullPath(options.utilityPath)
  options.certPath = mochitest.getFullPath(options.certPath)
  if options.symbolsPath and not isURL(options.symbolsPath):
    options.symbolsPath = mochitest.getFullPath(options.symbolsPath)

  sys.exit(mochitest.runTests(options))

if __name__ == "__main__":
  main()
