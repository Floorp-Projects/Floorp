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
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Robert Sayre <sayrer@gmail.com>
#   Jeff Walden <jwalden+bmo@mit.edu>
#   Serge Gautherie <sgautherie.bz@free.fr>
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
# ***** END LICENSE BLOCK *****

"""
Runs the Mochitest test harness.
"""

from __future__ import with_statement
from datetime import datetime
import optparse
import os
import os.path
import sys
import time

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))
sys.path.insert(0, SCRIPT_DIR);

import shutil
from urllib import quote_plus as encodeURIComponent
import urllib2
import commands
from automation import Automation
from automationutils import *
import tempfile

VMWARE_RECORDING_HELPER_BASENAME = "vmwarerecordinghelper"

#######################
# COMMANDLINE OPTIONS #
#######################

class MochitestOptions(optparse.OptionParser):
  """Parses Mochitest commandline options."""
  def __init__(self, automation, scriptdir, **kwargs):
    self._automation = automation
    optparse.OptionParser.__init__(self, **kwargs)
    defaults = {}

    # we want to pass down everything from self._automation.__all__
    addCommonOptions(self, defaults=dict(zip(self._automation.__all__, 
             [getattr(self._automation, x) for x in self._automation.__all__])))
    self._automation.addCommonOptions(self)

    self.add_option("--close-when-done",
                    action = "store_true", dest = "closeWhenDone",
                    help = "close the application when tests are done running")
    defaults["closeWhenDone"] = False

    self.add_option("--appname",
                    action = "store", type = "string", dest = "app",
                    help = "absolute path to application, overriding default")
    defaults["app"] = os.path.join(scriptdir, self._automation.DEFAULT_APP)

    self.add_option("--utility-path",
                    action = "store", type = "string", dest = "utilityPath",
                    help = "absolute path to directory containing utility programs (xpcshell, ssltunnel, certutil)")
    defaults["utilityPath"] = self._automation.DIST_BIN

    self.add_option("--certificate-path",
                    action = "store", type = "string", dest = "certPath",
                    help = "absolute path to directory containing certificate store to use testing profile")
    defaults["certPath"] = self._automation.CERTS_SRC_DIR

    self.add_option("--log-file",
                    action = "store", type = "string",
                    dest = "logFile", metavar = "FILE",
                    help = "file to which logging occurs")
    defaults["logFile"] = ""

    self.add_option("--autorun",
                    action = "store_true", dest = "autorun",
                    help = "start running tests when the application starts")
    defaults["autorun"] = False
    
    self.add_option("--timeout",
                    type = "int", dest = "timeout",
                    help = "per-test timeout in seconds")
    defaults["timeout"] = None

    self.add_option("--total-chunks",
                    type = "int", dest = "totalChunks",
                    help = "how many chunks to split the tests up into")
    defaults["totalChunks"] = None

    self.add_option("--this-chunk",
                    type = "int", dest = "thisChunk",
                    help = "which chunk to run")
    defaults["thisChunk"] = None

    self.add_option("--chunk-by-dir",
                    type = "int", dest = "chunkByDir",
                    help = "group tests together in the same chunk that are in the same top chunkByDir directories")
    defaults["chunkByDir"] = 0

    self.add_option("--shuffle",
                    dest = "shuffle",
                    action = "store_true",
                    help = "randomize test order")
    defaults["shuffle"] = False

    LOG_LEVELS = ("DEBUG", "INFO", "WARNING", "ERROR", "FATAL")
    LEVEL_STRING = ", ".join(LOG_LEVELS)

    self.add_option("--console-level",
                    action = "store", type = "choice", dest = "consoleLevel",
                    choices = LOG_LEVELS, metavar = "LEVEL",
                    help = "one of %s to determine the level of console "
                           "logging" % LEVEL_STRING)
    defaults["consoleLevel"] = None

    self.add_option("--file-level", 
                    action = "store", type = "choice", dest = "fileLevel",
                    choices = LOG_LEVELS, metavar = "LEVEL",
                    help = "one of %s to determine the level of file "
                           "logging if a file has been specified, defaulting "
                           "to INFO" % LEVEL_STRING)
    defaults["fileLevel"] = "INFO"

    self.add_option("--chrome",
                    action = "store_true", dest = "chrome",
                    help = "run chrome Mochitests")
    defaults["chrome"] = False

    self.add_option("--ipcplugins",
                    action = "store_true", dest = "ipcplugins",
                    help = "run ipcplugins Mochitests")
    defaults["ipcplugins"] = False

    self.add_option("--test-path",
                    action = "store", type = "string", dest = "testPath",
                    help = "start in the given directory's tests")
    defaults["testPath"] = ""

    self.add_option("--browser-chrome",
                    action = "store_true", dest = "browserChrome",
                    help = "run browser chrome Mochitests")
    defaults["browserChrome"] = False

    self.add_option("--a11y",
                    action = "store_true", dest = "a11y",
                    help = "run accessibility Mochitests");
    defaults["a11y"] = False

    self.add_option("--setenv",
                    action = "append", type = "string",
                    dest = "environment", metavar = "NAME=VALUE",
                    help = "sets the given variable in the application's "
                           "environment")
    defaults["environment"] = []
    
    self.add_option("--browser-arg",
                    action = "append", type = "string",
                    dest = "browserArgs", metavar = "ARG",
                    help = "provides an argument to the test application")
    defaults["browserArgs"] = []

    self.add_option("--leak-threshold",
                    action = "store", type = "int",
                    dest = "leakThreshold", metavar = "THRESHOLD",
                    help = "fail if the number of bytes leaked through "
                           "refcounted objects (or bytes in classes with "
                           "MOZ_COUNT_CTOR and MOZ_COUNT_DTOR) is greater "
                           "than the given number")
    defaults["leakThreshold"] = 0

    self.add_option("--fatal-assertions",
                    action = "store_true", dest = "fatalAssertions",
                    help = "abort testing whenever an assertion is hit "
                           "(requires a debug build to be effective)")
    defaults["fatalAssertions"] = False

    self.add_option("--extra-profile-file",
                    action = "append", dest = "extraProfileFiles",
                    help = "copy specified files/dirs to testing profile")
    defaults["extraProfileFiles"] = []

    self.add_option("--install-extension",
                    action = "append", dest = "extensionsToInstall",
                    help = "install the specified extension in the testing profile."
                           "The extension file's name should be <id>.xpi where <id> is"
                           "the extension's id as indicated in its install.rdf."
                           "An optional path can be specified too.")
    defaults["extensionsToInstall"] = []

    self.add_option("--profile-path", action = "store",
                    type = "string", dest = "profilePath",
                    help = "Directory where the profile will be stored."
                           "This directory will be deleted after the tests are finished")
    defaults["profilePath"] = tempfile.mkdtemp()

    self.add_option("--use-vmware-recording",
                    action = "store_true", dest = "vmwareRecording",
                    help = "enables recording while the application is running "
                           "inside a VMware Workstation 7.0 or later VM")
    defaults["vmwareRecording"] = False

    self.add_option("--repeat",
                    action = "store", type = "int",
                    dest = "repeat", metavar = "REPEAT",
                    help = "repeats the test or set of tests the given number of times, ie: repeat=1 will run the test twice.")
                   
    defaults["repeat"] = 0

    # -h, --help are automatically handled by OptionParser

    self.set_defaults(**defaults)

    usage = """\
Usage instructions for runtests.py.
All arguments are optional.
If --chrome is specified, chrome tests will be run instead of web content tests.
If --browser-chrome is specified, browser-chrome tests will be run instead of web content tests.
See <http://mochikit.com/doc/html/MochiKit/Logging.html> for details on the logging levels."""
    self.set_usage(usage)

  def verifyOptions(self, options, mochitest):
    """ verify correct options and cleanup paths """

    if options.totalChunks is not None and options.thisChunk is None:
      self.error("thisChunk must be specified when totalChunks is specified")

    if options.totalChunks:
      if not 1 <= options.thisChunk <= options.totalChunks:
        self.error("thisChunk must be between 1 and totalChunks")

    if options.xrePath is None:
      # default xrePath to the app path if not provided
      # but only if an app path was explicitly provided
      if options.app != self.defaults['app']:
        options.xrePath = os.path.dirname(options.app)
      else:
        # otherwise default to dist/bin
        options.xrePath = self._automation.DIST_BIN

    # allow relative paths
    options.xrePath = mochitest.getFullPath(options.xrePath)

    options.profilePath = mochitest.getFullPath(options.profilePath)

    options.app = mochitest.getFullPath(options.app)
    if not os.path.exists(options.app):
      msg = """\
      Error: Path %(app)s doesn't exist.
      Are you executing $objdir/_tests/testing/mochitest/runtests.py?"""
      print msg % {"app": options.app}
      return None

    options.utilityPath = mochitest.getFullPath(options.utilityPath)
    options.certPath = mochitest.getFullPath(options.certPath)
    if options.symbolsPath and not isURL(options.symbolsPath):
      options.symbolsPath = mochitest.getFullPath(options.symbolsPath)

    options.webServer = self._automation.DEFAULT_WEB_SERVER
    options.httpPort = self._automation.DEFAULT_HTTP_PORT
    options.sslPort = self._automation.DEFAULT_SSL_PORT
    options.webSocketPort = self._automation.DEFAULT_WEBSOCKET_PORT

    if options.vmwareRecording:
      if not self._automation.IS_WIN32:
        self.error("use-vmware-recording is only supported on Windows.")
      mochitest.vmwareHelperPath = os.path.join(
        options.utilityPath, VMWARE_RECORDING_HELPER_BASENAME + ".dll")
      if not os.path.exists(mochitest.vmwareHelperPath):
        self.error("%s not found, cannot automate VMware recording." %
                   mochitest.vmwareHelperPath)

    return options


#######################
# HTTP SERVER SUPPORT #
#######################

class MochitestServer:
  "Web server used to serve Mochitests, for closer fidelity to the real web."

  def __init__(self, automation, options):
    self._automation = automation
    self._closeWhenDone = options.closeWhenDone
    self._utilityPath = options.utilityPath
    self._xrePath = options.xrePath
    self._profileDir = options.profilePath
    self.webServer = options.webServer
    self.httpPort = options.httpPort
    self.shutdownURL = "http://%(server)s:%(port)s/server/shutdown" % { "server" : self.webServer, "port" : self.httpPort }

  def start(self):
    "Run the Mochitest server, returning the process ID of the server."
    
    env = self._automation.environment(xrePath = self._xrePath)
    env["XPCOM_DEBUG_BREAK"] = "warn"
    if self._automation.IS_WIN32:
      env["PATH"] = env["PATH"] + ";" + self._xrePath

    args = ["-g", self._xrePath,
            "-v", "170",
            "-f", "./" + "httpd.js",
            "-e", "const _PROFILE_PATH = '%(profile)s';const _SERVER_PORT = '%(port)s'; const _SERVER_ADDR ='%(server)s';" % 
                   {"profile" : self._profileDir.replace('\\', '\\\\'), "port" : self.httpPort, "server" : self.webServer },
            "-f", "./" + "server.js"]

    xpcshell = os.path.join(self._utilityPath,
                            "xpcshell" + self._automation.BIN_SUFFIX)
    self._process = self._automation.Process([xpcshell] + args, env = env)
    pid = self._process.pid
    if pid < 0:
      print "Error starting server."
      sys.exit(2)
    self._automation.log.info("INFO | runtests.py | Server pid: %d", pid)

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
      print "Timed out while waiting for server startup."
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
    cmd += ['-p', str(self.port), '-w', self._scriptdir, '-l', os.path.join(self._scriptdir, "websock.log"), '--log-level=debug']

    self._process = self._automation.Process(cmd)
    pid = self._process.pid
    if pid < 0:
      print "Error starting websocket server."
      sys.exit(2)
    self._automation.log.info("INFO | runtests.py | Websocket server pid: %d", pid)

  def stop(self):
    self._process.kill()

class Mochitest(object):
  # Path to the test script on the server
  TEST_PATH = "tests"
  CHROME_PATH = "redirect.html"
  PLAIN_LOOP_PATH = "plain-loop.html"
  urlOpts = []
  runSSLTunnel = True
  vmwareHelper = None

  oldcwd = os.getcwd()

  def __init__(self, automation):
    self.automation = automation

    # Max time in seconds to wait for server startup before tests will fail -- if
    # this seems big, it's mostly for debug machines where cold startup
    # (particularly after a build) takes forever.
    if self.automation.IS_DEBUG_BUILD:
      self.SERVER_STARTUP_TIMEOUT = 180
    else:
      self.SERVER_STARTUP_TIMEOUT = 90

    self.SCRIPT_DIRECTORY = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
    os.chdir(self.SCRIPT_DIRECTORY)

  def getFullPath(self, path):
    " Get an absolute path relative to self.oldcwd."
    return os.path.normpath(os.path.join(self.oldcwd, os.path.expanduser(path)))

  def buildTestPath(self, options):
    """ Build the url path to the specific test harness and test file or directory """
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
                                    self.SCRIPT_DIRECTORY, debuggerInfo)
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

  def getLogFilePath(self, logFile):
    """ return the log file path relative to the device we are testing on, in most cases 
        it will be the full path on the local system
    """
    return self.getFullPath(logFile)

  def installSpecialPowersExtension(self, options):
    """ install the Special Powers extension for special testing capabilities """
    extensionSource = os.path.normpath(os.path.join(self.SCRIPT_DIRECTORY, "specialpowers"))
    self.automation.log.info("INFO | runtests.py | Installing extension at %s to %s." % 
                             (extensionSource, options.profilePath))

    self.automation.installExtension(extensionSource, options.profilePath, "special-powers@mozilla.org")
    self.automation.log.info("INFO | runtests.py | Done installing extension.")

  def buildProfile(self, options):
    """ create the profile and add optional chrome bits and files if requested """
    self.automation.initializeProfile(options.profilePath, options.extraPrefs, useServerLocations = True)
    manifest = self.addChromeToProfile(options)
    self.copyExtraFilesToProfile(options)

    self.installSpecialPowersExtension(options)
    self.installExtensionsToProfile(options)
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
        print "Error: syntax error in --setenv=" + v
        return None
      browserEnv[v[:ix]] = v[ix + 1:]

    browserEnv["XPCOM_MEM_BLOAT_LOG"] = self.leak_report_file

    if options.fatalAssertions:
      browserEnv["XPCOM_DEBUG_BREAK"] = "stack-and-abort"

    return browserEnv

  def buildURLOptions(self, options, env):
    """ Add test control options from the command line to the url 

        URL parameters to test URL:

        autorun -- kick off tests automatically
        closeWhenDone -- runs quit.js after tests
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
    if options.browserChrome or options.chrome or options.a11y:
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
      if options.repeat:
        self.urlOpts.append("repeat=%d" % options.repeat)
      if os.path.isfile(os.path.join(self.oldcwd, os.path.dirname(__file__), self.TEST_PATH, options.testPath)) and options.repeat > 0:
        self.urlOpts.append("testname=%s" % ("/").join([self.TEST_PATH, options.testPath]))

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
      self.automation.log.warning("WARNING | runtests.py | Failed to load "
                                  "VMware recording helper")
      return
    self.automation.log.info("INFO | runtests.py | Starting VMware recording.")
    try:
      self.vmwareHelper.StartRecording()
    except Exception, e:
      self.automation.log.warning("WARNING | runtests.py | Failed to start "
                                  "VMware recording: (%s)" % str(e))
      self.vmwareHelper = None

  def stopVMwareRecording(self):
    """ stops recording inside VMware VM using the recording helper dll """
    assert(self.automation.IS_WIN32)
    if self.vmwareHelper is not None:
      self.automation.log.info("INFO | runtests.py | Stopping VMware "
                               "recording.")
      try:
        self.vmwareHelper.StopRecording()
      except Exception, e:
        self.automation.log.warning("WARNING | runtests.py | Failed to stop "
                                    "VMware recording: (%s)" % str(e))
      self.vmwareHelper = None

  def runTests(self, options):
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

    # Remove the leak detection file so it can't "leak" to the tests run.
    # The file is not there if leak logging was not enabled in the application build.
    if os.path.exists(self.leak_report_file):
      os.remove(self.leak_report_file)

    # then again to actually run mochitest
    if options.timeout:
      timeout = options.timeout + 30
    elif not options.autorun:
      timeout = None
    else:
      timeout = 330.0 # default JS harness timeout is 300 seconds

    if options.vmwareRecording:
      self.startVMwareRecording(options);

    self.automation.log.info("INFO | runtests.py | Running tests: start.\n")
    try:
      status = self.automation.runApp(testURL, browserEnv, options.app,
                                  options.profilePath, options.browserArgs,
                                  runSSLTunnel = self.runSSLTunnel,
                                  utilityPath = options.utilityPath,
                                  xrePath = options.xrePath,
                                  certPath=options.certPath,
                                  debuggerInfo=debuggerInfo,
                                  symbolsPath=options.symbolsPath,
                                  timeout = timeout)
    except KeyboardInterrupt:
      self.automation.log.info("INFO | runtests.py | Received keyboard interrupt.\n");
      status = -1

    if options.vmwareRecording:
      self.stopVMwareRecording();

    self.stopWebServer(options)
    self.stopWebSocketServer(options)
    processLeakLog(self.leak_report_file, options.leakThreshold)
    self.automation.log.info("\nINFO | runtests.py | Running tests: end.")

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
    testRoot = 'chrome'
    if (options.browserChrome):
      testRoot = 'browser'
    elif (options.a11y):
      testRoot = 'a11y'
 
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

    # Call installChromeJar().
    jarDir = "mochijar"
    if not os.path.isdir(os.path.join(self.SCRIPT_DIRECTORY, jarDir)):
      self.automation.log.warning("TEST-UNEXPECTED-FAIL | invalid setup: missing mochikit extension")
      return None

    # Support Firefox (browser) and SeaMonkey (navigator).
    chrome = ""
    if options.browserChrome or options.chrome or options.a11y:
      chrome += """
overlay chrome://browser/content/browser.xul chrome://mochikit/content/browser-test-overlay.xul
overlay chrome://navigator/content/navigator.xul chrome://mochikit/content/browser-test-overlay.xul
"""
    else:
      #only do the ipc-overlay.xul for mochitest-plain.  
      #Currently there are focus issues in chrome tests and issues with new windows and dialogs when using ipc
      chrome += """
overlay chrome://browser/content/browser.xul chrome://mochikit/content/ipc-overlay.xul
overlay chrome://navigator/content/navigator.xul chrome://mochikit/content/ipc-overlay.xul
"""

    self.installChromeJar(jarDir, chrome, options)

    return manifest

  def installChromeJar(self, jarDirName, chrome, options):
    """
      copy mochijar directory to profile as an extension so we have chrome://mochikit for all harness code
    """
    self.automation.installExtension(os.path.join(self.SCRIPT_DIRECTORY, jarDirName), \
                                     options.profilePath, "mochikit@mozilla.org")

    # Write chrome.manifest.
    with open(os.path.join(options.profilePath, "extensions", "mochikit@mozilla.org", "chrome.manifest"), "a") as mfile:
      mfile.write(chrome)

  def copyTestsJarToProfile(self, options):
    """ copy tests.jar to the profile directory so we can auto register it in the .xul harness """
    testsJarFile = os.path.join(self.SCRIPT_DIRECTORY, "tests.jar")
    if not os.path.isfile(testsJarFile):
      return False

    shutil.copy2(testsJarFile, options.profilePath)
    return True

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
        self.automation.log.warning("WARNING | runtests.py | Failed to copy %s to profile", abspath)
        continue

  def installExtensionsToProfile(self, options):
    "Install application distributed extensions and specified on the command line ones to testing profile."
    # Install distributed extensions, if application has any.
    distExtDir = os.path.join(options.app[ : options.app.rfind(os.sep)], "distribution", "extensions")
    if os.path.isdir(distExtDir):
      for f in os.listdir(distExtDir):
        self.automation.installExtension(os.path.join(distExtDir, f), options.profilePath)

    # Install custom extensions.
    for f in options.extensionsToInstall:
      self.automation.installExtension(self.getFullPath(f), options.profilePath)

def main():
  automation = Automation()
  mochitest = Mochitest(automation)
  parser = MochitestOptions(automation, mochitest.SCRIPT_DIRECTORY)
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
