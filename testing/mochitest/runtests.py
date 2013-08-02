#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the Mochitest test harness.
"""

from __future__ import with_statement
import optparse
import os
import sys
import time
import traceback

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
sys.path.insert(0, SCRIPT_DIR);

import shutil
from urllib import quote_plus as encodeURIComponent
import urllib2
from automation import Automation
from automationutils import getDebuggerInfo, isURL, processLeakLog
from mochitest_options import MochitestOptions
from manifestparser import TestManifest
import mozinfo
import json

import mozlog

log = mozlog.getLogger('Mochitest')


#######################
# HTTP SERVER SUPPORT #
#######################

class MochitestServer:
  "Web server used to serve Mochitests, for closer fidelity to the real web."

  def __init__(self, automation, options):
    if isinstance(options, optparse.Values):
      options = vars(options)
    self._automation = automation or Automation()
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

    env = self._automation.environment(xrePath = self._xrePath)
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
            "-e", """const _PROFILE_PATH = '%(profile)s';const _SERVER_PORT = '%(port)s'; const _SERVER_ADDR = '%(server)s';
                     const _TEST_PREFIX = %(testPrefix)s; const _DISPLAY_RESULTS = %(displayResults)s;""" %
                   {"profile" : self._profileDir.replace('\\', '\\\\'), "port" : self.httpPort, "server" : self.webServer,
                    "testPrefix" : self.testPrefix, "displayResults" : str(not self._closeWhenDone).lower() },
            "-f", "./" + "server.js"]

    xpcshell = os.path.join(self._utilityPath,
                            "xpcshell" + mozinfo.info['bin_suffix'])
    self._process = self._automation.Process([xpcshell] + args, env = env)
    pid = self._process.pid
    if pid < 0:
      log.error("Error starting server.")
      sys.exit(2)
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
      log.error("Timed out while waiting for server startup.")
      self.stop()
      sys.exit(1)

  def stop(self):
    try:
      with urllib2.urlopen(self.shutdownURL) as c:
        c.read()

      rtncode = self._process.poll()
      if rtncode is None:
        self._process.terminate()
    except:
      self._process.kill()

class WebSocketServer(object):
  "Class which encapsulates the mod_pywebsocket server"

  def __init__(self, automation, options, scriptdir, debuggerInfo=None):
    self.port = options.webSocketPort
    self._automation = automation
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

    self._process = self._automation.Process(cmd)
    pid = self._process.pid
    if pid < 0:
      log.error("Error starting websocket server.")
      sys.exit(2)
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
    mozinfo.find_and_update_from_json(SCRIPT_DIR)

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

    self.wsserver = WebSocketServer(self.automation, options,
                                    SCRIPT_DIR, debuggerInfo)
    self.wsserver.start()

  def stopWebSocketServer(self, options):
    if options.webServer != '127.0.0.1':
      return

    self.wsserver.stop()

  def startWebServer(self, options):
    if options.webServer != '127.0.0.1':
      return

    """ Create the webserver and start it up """
    self.server = MochitestServer(self.automation, options)
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
        continue

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
        if self.automation.IS_WIN32:
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
overlay chrome://browser/content/shell.xul chrome://mochikit/content/browser-test-overlay.xul
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

  def __init__(self, automation=None):
    super(Mochitest, self).__init__()
    self.automation = automation or Automation()

    # Max time in seconds to wait for server startup before tests will fail -- if
    # this seems big, it's mostly for debug machines where cold startup
    # (particularly after a build) takes forever.
    if self.automation.IS_DEBUG_BUILD:
      self.SERVER_STARTUP_TIMEOUT = 180
    else:
      self.SERVER_STARTUP_TIMEOUT = 90

  def buildProfile(self, options):
    """ create the profile and add optional chrome bits and files if requested """
    if options.browserChrome and options.timeout:
      options.extraPrefs.append("testing.browserTestHarness.timeout=%d" % options.timeout)

    # get extensions to install
    extensions = self.getExtensionsToInstall(options)

    # create a profile
    appsPath = os.path.join(SCRIPT_DIR, 'profile_data', 'webapps_mochitest.json')
    appsPath = appsPath if os.path.exists(appsPath) else None
    prefsPath = os.path.join(SCRIPT_DIR, 'profile_data', 'prefs_general.js')
    profile = self.automation.initializeProfile(options.profilePath,
                                                options.extraPrefs,
                                                useServerLocations=True,
                                                appsPath=appsPath,
                                                prefsPath=prefsPath,
                                                addons=extensions)

    #if we don't do this, the profile object is destroyed when we exit this method
    self.profile = profile
    options.profilePath = profile.profile

    manifest = self.addChromeToProfile(options)
    self.copyExtraFilesToProfile(options)
    return manifest

  def buildBrowserEnv(self, options):
    """ build the environment variables for the specific test and operating system """
    browserEnv = self.automation.environment(xrePath = options.xrePath)

    # These variables are necessary for correct application startup; change
    # via the commandline at your own risk.
    browserEnv["XPCOM_DEBUG_BREAK"] = "stack"

    for v in options.environment:
      ix = v.find("=")
      if ix <= 0:
        log.error("Error: syntax error in --setenv=" + v)
        return None
      browserEnv[v[:ix]] = v[ix + 1:]

    browserEnv["XPCOM_MEM_BLOAT_LOG"] = self.leak_report_file

    if options.fatalAssertions:
      browserEnv["XPCOM_DEBUG_BREAK"] = "stack-and-abort"

    return browserEnv

  def cleanup(self, manifest, options):
    """ remove temporary files and profile """
    os.remove(manifest)
    shutil.rmtree(options.profilePath)

  def startVMwareRecording(self, options):
    """ starts recording inside VMware VM using the recording helper dll """
    assert(self.automation.IS_WIN32)
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
    assert(self.automation.IS_WIN32)
    if self.vmwareHelper is not None:
      log.info("runtests.py | Stopping VMware "
               "recording.")
      try:
        self.vmwareHelper.StopRecording()
      except Exception, e:
        log.warning("runtests.py | Failed to stop "
                    "VMware recording: (%s)" % str(e))
      self.vmwareHelper = None

  def runTests(self, options, onLaunch=None):
    """ Prepare, configure, run tests and cleanup """
    debuggerInfo = getDebuggerInfo(self.oldcwd, options.debugger, options.debuggerArgs,
                      options.debuggerInteractive);

    self.leak_report_file = os.path.join(options.profilePath, "runtests_leaks.log")

    browserEnv = self.buildBrowserEnv(options)
    if browserEnv is None:
      return 1

    manifest = self.buildProfile(options)
    if manifest is None:
      return 1

    self.startWebServer(options)
    self.startWebSocketServer(options, debuggerInfo)

    testURL = self.buildTestPath(options)
    self.buildURLOptions(options, browserEnv)
    if len(self.urlOpts) > 0:
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
      status = self.automation.runApp(testURL, browserEnv, options.app,
                                  options.profilePath, options.browserArgs,
                                  runSSLTunnel=self.runSSLTunnel,
                                  utilityPath=options.utilityPath,
                                  xrePath=options.xrePath,
                                  certPath=options.certPath,
                                  debuggerInfo=debuggerInfo,
                                  symbolsPath=options.symbolsPath,
                                  timeout=timeout,
                                  onLaunch=onLaunch)
    except KeyboardInterrupt:
      log.info("runtests.py | Received keyboard interrupt.\n");
      status = -1
    except:
      traceback.print_exc()
      log.error("runtests.py | Received unexpected exception while running application\n")
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
    extensionPath = self.getFullPath(path)

    log.info("runtests.py | Installing extension at %s to %s." %
                (extensionPath, options.profilePath))
    self.automation.installExtension(extensionPath, options.profilePath,
                                     extensionID)

  def installExtensionsToProfile(self, options):
    "Install special testing extensions, application distributed extensions, and specified on the command line ones to testing profile."
    for path in self.getExtensionsToInstall(options):
      self.installExtensionFromPath(options, path)

def main():
  automation = Automation()
  mochitest = Mochitest(automation)
  parser = MochitestOptions(automation)
  options, args = parser.parse_args()

  options = parser.verifyOptions(options, mochitest)
  if options == None:
    sys.exit(1)

  options.utilityPath = mochitest.getFullPath(options.utilityPath)
  options.certPath = mochitest.getFullPath(options.certPath)
  if options.symbolsPath and not isURL(options.symbolsPath):
    options.symbolsPath = mochitest.getFullPath(options.symbolsPath)

  automation.setServerInfo(options.webServer,
                           options.httpPort,
                           options.sslPort,
                           options.webSocketPort)
  sys.exit(mochitest.runTests(options))

if __name__ == "__main__":
  main()
