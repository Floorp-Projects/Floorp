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
sys.path.insert(0, SCRIPT_DIR)

from argparse import Namespace
from urlparse import urlparse
import ctypes
import glob
import json
import mozcrash
import mozdebug
import mozinfo
import mozprocess
import mozrunner
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import traceback
import urllib2
import zipfile
import bisection

from automationutils import (
    environment,
    processLeakLog,
    dumpScreen,
    ShutdownLeaks,
    printstatus,
    LSANLeaks,
    setAutomationLog,
)

from datetime import datetime
from manifestparser import TestManifest
from manifestparser.filters import (
    chunk_by_dir,
    chunk_by_runtime,
    chunk_by_slice,
    subsuite,
    tags,
)
from mochitest_options import MochitestOptions
from mozprofile import Profile, Preferences
from mozprofile.permissions import ServerLocations
from urllib import quote_plus as encodeURIComponent
from mozlog.structured.formatters import TbplFormatter
from mozlog.structured import commandline


###########################
# Option for NSPR logging #
###########################

# Set the desired log modules you want an NSPR log be produced by a try run for, or leave blank to disable the feature.
# This will be passed to NSPR_LOG_MODULES environment variable. Try run will then put a download link for the log file
# on tbpl.mozilla.org.

NSPR_LOG_MODULES = ""

####################
# LOG HANDLING     #
####################

# output processing


class MochitestFormatter(TbplFormatter):

    """
    The purpose of this class is to maintain compatibility with legacy users.
    Mozharness' summary parser expects the count prefix, and others expect python
    logging to contain a line prefix picked up by TBPL (bug 1043420).
    Those directly logging "TEST-UNEXPECTED" require no prefix to log output
    in order to turn a build orange (bug 1044206).

    Once updates are propagated to Mozharness, this class may be removed.
    """
    log_num = 0

    def __init__(self):
        super(MochitestFormatter, self).__init__()

    def __call__(self, data):
        output = super(MochitestFormatter, self).__call__(data)
        log_level = data.get('level', 'info').upper()

        if 'js_source' in data or log_level == 'ERROR':
            data.pop('js_source', None)
            output = '%d %s %s' % (
                MochitestFormatter.log_num, log_level, output)
            MochitestFormatter.log_num += 1

        return output

# output processing


class MessageLogger(object):

    """File-like object for logging messages (structured logs)"""
    BUFFERING_THRESHOLD = 100
    # This is a delimiter used by the JS side to avoid logs interleaving
    DELIMITER = u'\ue175\uee31\u2c32\uacbf'
    BUFFERED_ACTIONS = set(['test_status', 'log'])
    VALID_ACTIONS = set(['suite_start', 'suite_end', 'test_start', 'test_end',
                         'test_status', 'log',
                         'buffering_on', 'buffering_off'])
    TEST_PATH_PREFIXES = ['/tests/',
                          'chrome://mochitests/content/browser/',
                          'chrome://mochitests/content/chrome/']

    def __init__(self, logger, buffering=True):
        self.logger = logger
        self.buffering = buffering
        self.restore_buffering = False
        self.tests_started = False

        # Message buffering
        self.buffered_messages = []

        # Failures reporting, after the end of the tests execution
        self.errors = []

    def valid_message(self, obj):
        """True if the given object is a valid structured message (only does a superficial validation)"""
        return isinstance(obj, dict) and 'action' in obj and obj[
            'action'] in MessageLogger.VALID_ACTIONS

    def _fix_test_name(self, message):
        """Normalize a logged test path to match the relative path from the sourcedir.
        """
        if 'test' in message:
            test = message['test']
            for prefix in MessageLogger.TEST_PATH_PREFIXES:
                if test.startswith(prefix):
                    message['test'] = test[len(prefix):]
                    break

    def parse_line(self, line):
        """Takes a given line of input (structured or not) and returns a list of structured messages"""
        line = line.rstrip().decode("UTF-8", "replace")

        messages = []
        for fragment in line.split(MessageLogger.DELIMITER):
            if not fragment:
                continue
            try:
                message = json.loads(fragment)
                if not self.valid_message(message):
                    message = dict(
                        action='log',
                        level='info',
                        message=fragment,
                        unstructured=True)
            except ValueError:
                message = dict(
                    action='log',
                    level='info',
                    message=fragment,
                    unstructured=True)
            self._fix_test_name(message)
            messages.append(message)

        return messages

    def process_message(self, message):
        """Processes a structured message. Takes into account buffering, errors, ..."""
        if not self.tests_started and message['action'] == 'test_start':
            self.tests_started = True

        # Activation/deactivating message buffering from the JS side
        if message['action'] == 'buffering_on':
            self.buffering = True
            return
        if message['action'] == 'buffering_off':
            self.buffering = False
            return

        unstructured = False
        if 'unstructured' in message:
            unstructured = True
            message.pop('unstructured')

        # Error detection also supports "raw" errors (in log messages) because some tests
        # manually dump 'TEST-UNEXPECTED-FAIL'.
        if ('expected' in message or (message['action'] == 'log' and message[
                'message'].startswith('TEST-UNEXPECTED'))):
            # Saving errors/failures to be shown at the end of the test run
            self.errors.append(message)
            self.restore_buffering = self.restore_buffering or self.buffering
            self.buffering = False
            if self.buffered_messages:
                snipped = len(
                    self.buffered_messages) - self.BUFFERING_THRESHOLD
                if snipped > 0:
                    self.logger.info(
                        "<snipped {0} output lines - "
                        "if you need more context, please use "
                        "SimpleTest.requestCompleteLog() in your test>" .format(snipped))
                # Dumping previously buffered messages
                self.dump_buffered(limit=True)

            # Logging the error message
            self.logger.log_raw(message)
        # If we don't do any buffering, or the tests haven't started, or the message was
        # unstructured, it is directly logged.
        elif any([not self.buffering,
                  unstructured,
                  not self.tests_started,
                  message['action'] not in self.BUFFERED_ACTIONS]):
            self.logger.log_raw(message)
        else:
            # Buffering the message
            self.buffered_messages.append(message)

        # If a test ended, we clean the buffer
        if message['action'] == 'test_end':
            self.buffered_messages = []
            if self.restore_buffering:
                self.restore_buffering = False
                self.buffering = True

    def write(self, line):
        messages = self.parse_line(line)
        for message in messages:
            self.process_message(message)
        return messages

    def flush(self):
        sys.stdout.flush()

    def dump_buffered(self, limit=False):
        if limit:
            dumped_messages = self.buffered_messages[-
                                                     self.BUFFERING_THRESHOLD:]
        else:
            dumped_messages = self.buffered_messages

        for buf_msg in dumped_messages:
            self.logger.log_raw(buf_msg)
        # Cleaning the list of buffered messages
        self.buffered_messages = []

    def finish(self):
        self.dump_buffered()
        self.buffering = False
        self.logger.suite_end()

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


def killPid(pid, log):
    # see also https://bugzilla.mozilla.org/show_bug.cgi?id=911249#c58
    try:
        os.kill(pid, getattr(signal, "SIGKILL", signal.SIGTERM))
    except Exception as e:
        log.info("Failed to kill process %d: %s" % (pid, str(e)))

if mozinfo.isWin:
    import ctypes.wintypes

    def isPidAlive(pid):
        STILL_ACTIVE = 259
        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
        pHandle = ctypes.windll.kernel32.OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            0,
            pid)
        if not pHandle:
            return False
        pExitCode = ctypes.wintypes.DWORD()
        ctypes.windll.kernel32.GetExitCodeProcess(
            pHandle,
            ctypes.byref(pExitCode))
        ctypes.windll.kernel32.CloseHandle(pHandle)
        return pExitCode.value == STILL_ACTIVE

else:
    import errno

    def isPidAlive(pid):
        try:
            # kill(pid, 0) checks for a valid PID without actually sending a signal
            # The method throws OSError if the PID is invalid, which we catch
            # below.
            os.kill(pid, 0)

            # Wait on it to see if it's a zombie. This can throw OSError.ECHILD if
            # the process terminates before we get to this point.
            wpid, wstatus = os.waitpid(pid, os.WNOHANG)
            return wpid == 0
        except OSError as err:
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

    def __init__(self, options, logger):
        if isinstance(options, Namespace):
            options = vars(options)
        self._log = logger
        self._closeWhenDone = options['closeWhenDone']
        self._utilityPath = options['utilityPath']
        self._xrePath = options['xrePath']
        self._profileDir = options['profilePath']
        self.webServer = options['webServer']
        self.httpPort = options['httpPort']
        self.shutdownURL = "http://%(server)s:%(port)s/server/shutdown" % {
            "server": self.webServer,
            "port": self.httpPort}
        self.testPrefix = "'webapprt_'" if options.get(
            'webapprtContent') else "undefined"

        if options.get('httpdPath'):
            self._httpdPath = options['httpdPath']
        else:
            self._httpdPath = SCRIPT_DIR
        self._httpdPath = os.path.abspath(self._httpdPath)

    def start(self):
        "Run the Mochitest server, returning the process ID of the server."

        # get testing environment
        env = environment(xrePath=self._xrePath)
        env["XPCOM_DEBUG_BREAK"] = "warn"
        env["LD_LIBRARY_PATH"] = self._xrePath

        # When running with an ASan build, our xpcshell server will also be ASan-enabled,
        # thus consuming too much resources when running together with the browser on
        # the test slaves. Try to limit the amount of resources by disabling certain
        # features.
        env["ASAN_OPTIONS"] = "quarantine_size=1:redzone=32:malloc_context_size=5"

        # Likewise, when running with a TSan build, our xpcshell server will
        # also be TSan-enabled. Except that in this case, we don't really
        # care about races in xpcshell. So disable TSan for the server.
        env["TSAN_OPTIONS"] = "report_bugs=0"

        if mozinfo.isWin:
            env["PATH"] = env["PATH"] + ";" + str(self._xrePath)

        args = [
            "-g",
            self._xrePath,
            "-v",
            "170",
            "-f",
            os.path.join(
                self._httpdPath,
                "httpd.js"),
            "-e",
            """const _PROFILE_PATH = '%(profile)s'; const _SERVER_PORT = '%(port)s'; const _SERVER_ADDR = '%(server)s'; const _TEST_PREFIX = %(testPrefix)s; const _DISPLAY_RESULTS = %(displayResults)s;""" % {
                "profile": self._profileDir.replace(
                    '\\',
                    '\\\\'),
                "port": self.httpPort,
                "server": self.webServer,
                "testPrefix": self.testPrefix,
                "displayResults": str(
                    not self._closeWhenDone).lower()},
            "-f",
            os.path.join(
                SCRIPT_DIR,
                "server.js")]

        xpcshell = os.path.join(self._utilityPath,
                                "xpcshell" + mozinfo.info['bin_suffix'])
        command = [xpcshell] + args
        self._process = mozprocess.ProcessHandler(
            command,
            cwd=SCRIPT_DIR,
            env=env)
        self._process.run()
        self._log.info(
            "%s : launching %s" %
            (self.__class__.__name__, command))
        pid = self._process.pid
        self._log.info("runtests.py | Server pid: %d" % pid)

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
            self._log.info(
                "TEST-UNEXPECTED-FAIL | runtests.py | Timed out while waiting for server startup.")
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

    def __init__(self, options, scriptdir, logger, debuggerInfo=None):
        self.port = options.webSocketPort
        self.debuggerInfo = debuggerInfo
        self._log = logger
        self._scriptdir = scriptdir

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
        if self.debuggerInfo and self.debuggerInfo.interactive:
            cmd += ['--interactive']
        cmd += ['-p', str(self.port), '-w', self._scriptdir, '-l',
                os.path.join(self._scriptdir, "websock.log"),
                '--log-level=debug', '--allow-handlers-outside-root-dir']
        # start the process
        self._process = mozprocess.ProcessHandler(cmd, cwd=SCRIPT_DIR)
        self._process.run()
        pid = self._process.pid
        self._log.info("runtests.py | Websocket server pid: %d" % pid)

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
    NESTED_OOP_TEST_PATH = "nested_oop"
    CHROME_PATH = "redirect.html"
    urlOpts = []
    log = None

    def __init__(self, logger_options):
        self.update_mozinfo()
        self.server = None
        self.wsserver = None
        self.sslTunnel = None
        self._locations = None

        if self.log is None:
            commandline.log_formatters["tbpl"] = (
                MochitestFormatter,
                "Mochitest specific tbpl formatter")
            self.log = commandline.setup_logging("mochitest",
                                                 logger_options,
                                                 {
                                                     "tbpl": sys.stdout
                                                 })
            MochitestUtilsMixin.log = self.log
            setAutomationLog(self.log)

        self.message_logger = MessageLogger(logger=self.log)

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
        return os.path.normpath(
            os.path.join(
                self.oldcwd,
                os.path.expanduser(path)))

    def getLogFilePath(self, logFile):
        """ return the log file path relative to the device we are testing on, in most cases
            it will be the full path on the local system
        """
        return self.getFullPath(logFile)

    @property
    def locations(self):
        if self._locations is not None:
            return self._locations
        locations_file = os.path.join(SCRIPT_DIR, 'server-locations.txt')
        self._locations = ServerLocations(locations_file)
        return self._locations

    def buildURLOptions(self, options, env):
        """ Add test control options from the command line to the url

            URL parameters to test URL:

            autorun -- kick off tests automatically
            closeWhenDone -- closes the browser after the tests
            hideResultsTable -- hides the table of individual test results
            logFile -- logs test run to an absolute path
            startAt -- name of test to start at
            endAt -- name of test to end at
            timeout -- per-test timeout in seconds
            repeat -- How many times to repeat the test, ie: repeat=1 will run the test twice.
        """

        if not hasattr(options, 'logFile'):
            options.logFile = ""
        if not hasattr(options, 'fileLevel'):
            options.fileLevel = 'INFO'

        # allow relative paths for logFile
        if options.logFile:
            options.logFile = self.getLogFilePath(options.logFile)

        if options.browserChrome or options.chrome or \
           options.a11y or options.webapprtChrome or options.jetpackPackage or \
           options.jetpackAddon:
            self.makeTestConfig(options)
        else:
            if options.autorun:
                self.urlOpts.append("autorun=1")
            if options.timeout:
                self.urlOpts.append("timeout=%d" % options.timeout)
            if options.maxTimeouts:
                self.urlOpts.append("maxTimeouts=%d" % options.maxTimeouts)
            if options.closeWhenDone:
                self.urlOpts.append("closeWhenDone=1")
            if options.webapprtContent:
                self.urlOpts.append("testRoot=webapprtContent")
            if options.logFile:
                self.urlOpts.append(
                    "logFile=" +
                    encodeURIComponent(
                        options.logFile))
                self.urlOpts.append(
                    "fileLevel=" +
                    encodeURIComponent(
                        options.fileLevel))
            if options.consoleLevel:
                self.urlOpts.append(
                    "consoleLevel=" +
                    encodeURIComponent(
                        options.consoleLevel))
            if options.startAt:
                self.urlOpts.append("startAt=%s" % options.startAt)
            if options.endAt:
                self.urlOpts.append("endAt=%s" % options.endAt)
            if options.shuffle:
                self.urlOpts.append("shuffle=1")
            if "MOZ_HIDE_RESULTS_TABLE" in env and env[
                    "MOZ_HIDE_RESULTS_TABLE"] == "1":
                self.urlOpts.append("hideResultsTable=1")
            if options.runUntilFailure:
                self.urlOpts.append("runUntilFailure=1")
            if options.repeat:
                self.urlOpts.append("repeat=%d" % options.repeat)
            if os.path.isfile(
                os.path.join(
                    self.oldcwd,
                    os.path.dirname(__file__),
                    self.TEST_PATH,
                    options.testPath)) and options.repeat > 0:
                self.urlOpts.append("testname=%s" %
                                    ("/").join([self.TEST_PATH, options.testPath]))
            if options.manifestFile:
                self.urlOpts.append("manifestFile=%s" % options.manifestFile)
            if options.failureFile:
                self.urlOpts.append(
                    "failureFile=%s" %
                    self.getFullPath(
                        options.failureFile))
            if options.runSlower:
                self.urlOpts.append("runSlower=true")
            if options.debugOnFailure:
                self.urlOpts.append("debugOnFailure=true")
            if options.dumpOutputDirectory:
                self.urlOpts.append(
                    "dumpOutputDirectory=%s" %
                    encodeURIComponent(
                        options.dumpOutputDirectory))
            if options.dumpAboutMemoryAfterTest:
                self.urlOpts.append("dumpAboutMemoryAfterTest=true")
            if options.dumpDMDAfterTest:
                self.urlOpts.append("dumpDMDAfterTest=true")
            if options.debugger:
                self.urlOpts.append("interactiveDebugger=true")

    def getTestFlavor(self, options):
        if options.browserChrome:
            return "browser-chrome"
        elif options.jetpackPackage:
            return "jetpack-package"
        elif options.jetpackAddon:
            return "jetpack-addon"
        elif options.chrome:
            return "chrome"
        elif options.a11y:
            return "a11y"
        elif options.webapprtChrome:
            return "webapprt-chrome"
        elif options.webapprtContent:
            return "webapprt-content"
        else:
            return "mochitest"

    # This check can be removed when bug 983867 is fixed.
    def isTest(self, options, filename):
        allow_js_css = False
        if options.browserChrome:
            allow_js_css = True
            testPattern = re.compile(r"browser_.+\.js")
        elif options.jetpackPackage:
            allow_js_css = True
            testPattern = re.compile(r"test-.+\.js")
        elif options.jetpackAddon:
            testPattern = re.compile(r".+\.xpi")
        elif options.chrome or options.a11y:
            testPattern = re.compile(r"(browser|test)_.+\.(xul|html|js|xhtml)")
        elif options.webapprtContent:
            testPattern = re.compile(r"webapprt_")
        elif options.webapprtChrome:
            allow_js_css = True
            testPattern = re.compile(r"browser_")
        else:
            testPattern = re.compile(r"test_")

        if not allow_js_css and (".js" in filename or ".css" in filename):
            return False

        pathPieces = filename.split("/")

        return (testPattern.match(pathPieces[-1]) and
                not re.search(r'\^headers\^$', filename))

    def getTestPath(self, options):
        if options.ipcplugins:
            return "dom/plugins/test/mochitest"
        else:
            return options.testPath

    def setTestRoot(self, options):
        if hasattr(self, "testRoot"):
            return self.testRoot, self.testRootAbs
        else:
            if options.browserChrome:
                if options.immersiveMode:
                    self.testRoot = 'metro'
                else:
                    self.testRoot = 'browser'
            elif options.jetpackPackage:
                self.testRoot = 'jetpack-package'
            elif options.jetpackAddon:
                self.testRoot = 'jetpack-addon'
            elif options.a11y:
                self.testRoot = 'a11y'
            elif options.webapprtChrome:
                self.testRoot = 'webapprtChrome'
            elif options.webapprtContent:
                self.testRoot = 'webapprtContent'
            elif options.chrome:
                self.testRoot = 'chrome'
            else:
                self.testRoot = self.TEST_PATH
            self.testRootAbs = os.path.join(SCRIPT_DIR, self.testRoot)

    def buildTestURL(self, options):
        testHost = "http://mochi.test:8888"
        testPath = self.getTestPath(options)
        testURL = "/".join([testHost, self.TEST_PATH, testPath])
        if os.path.isfile(
            os.path.join(
                self.oldcwd,
                os.path.dirname(__file__),
                self.TEST_PATH,
                testPath)) and options.repeat > 0:
            testURL = "/".join([testHost,
                                self.TEST_PATH,
                                os.path.dirname(testPath)])
        if options.chrome or options.a11y:
            testURL = "/".join([testHost, self.CHROME_PATH])
        elif options.browserChrome or options.jetpackPackage or options.jetpackAddon:
            testURL = "about:blank"
        if options.nested_oop:
            testURL = "/".join([testHost, self.NESTED_OOP_TEST_PATH])
        return testURL

    def buildTestPath(self, options, testsToFilter=None, disabled=True):
        """ Build the url path to the specific test harness and test file or directory
            Build a manifest of tests to run and write out a json file for the harness to read
            testsToFilter option is used to filter/keep the tests provided in the list

            disabled -- This allows to add all disabled tests on the build side
                        and then on the run side to only run the enabled ones
        """

        tests = self.getActiveTests(options, disabled)
        paths = []
        for test in tests:
            if testsToFilter and (test['path'] not in testsToFilter):
                continue
            paths.append(test)

        # Bug 883865 - add this functionality into manifestparser
        with open(os.path.join(SCRIPT_DIR, options.testRunManifestFile), 'w') as manifestFile:
            manifestFile.write(json.dumps({'tests': paths}))
        options.manifestFile = options.testRunManifestFile

        return self.buildTestURL(options)

    def startWebSocketServer(self, options, debuggerInfo):
        """ Launch the websocket server """
        self.wsserver = WebSocketServer(
            options,
            SCRIPT_DIR,
            self.log,
            debuggerInfo)
        self.wsserver.start()

    def startWebServer(self, options):
        """Create the webserver and start it up"""

        self.server = MochitestServer(options, self.log)
        self.server.start()

        if options.pidFile != "":
            with open(options.pidFile + ".xpcshell.pid", 'w') as f:
                f.write("%s" % self.server._process.pid)

    def startServers(self, options, debuggerInfo, ignoreSSLTunnelExts=False):
        # start servers and set ports
        # TODO: pass these values, don't set on `self`
        self.webServer = options.webServer
        self.httpPort = options.httpPort
        self.sslPort = options.sslPort
        self.webSocketPort = options.webSocketPort

        # httpd-path is specified by standard makefile targets and may be specified
        # on the command line to select a particular version of httpd.js. If not
        # specified, try to select the one from hostutils.zip, as required in
        # bug 882932.
        if not options.httpdPath:
            options.httpdPath = os.path.join(options.utilityPath, "components")

        self.startWebServer(options)
        self.startWebSocketServer(options, debuggerInfo)

        # start SSL pipe
        self.sslTunnel = SSLTunnel(
            options,
            logger=self.log,
            ignoreSSLTunnelExts=ignoreSSLTunnelExts)
        self.sslTunnel.buildConfig(self.locations)
        self.sslTunnel.start()

        # If we're lucky, the server has fully started by now, and all paths are
        # ready, etc.  However, xpcshell cold start times suck, at least for debug
        # builds.  We'll try to connect to the server for awhile, and if we fail,
        # we'll try to kill the server and exit with an error.
        if self.server is not None:
            self.server.ensureReady(self.SERVER_STARTUP_TIMEOUT)

    def stopServers(self):
        """Servers are no longer needed, and perhaps more importantly, anything they
            might spew to console might confuse things."""
        if self.server is not None:
            try:
                self.log.info('Stopping web server')
                self.server.stop()
            except Exception:
                self.log.critical('Exception when stopping web server')

        if self.wsserver is not None:
            try:
                self.log.info('Stopping web socket server')
                self.wsserver.stop()
            except Exception:
                self.log.critical('Exception when stopping web socket server')

        if self.sslTunnel is not None:
            try:
                self.log.info('Stopping ssltunnel')
                self.sslTunnel.stop()
            except Exception:
                self.log.critical('Exception stopping ssltunnel')

    def copyExtraFilesToProfile(self, options):
        "Copy extra files or dirs specified on the command line to the testing profile."
        for f in options.extraProfileFiles:
            abspath = self.getFullPath(f)
            if os.path.isfile(abspath):
                shutil.copy2(abspath, options.profilePath)
            elif os.path.isdir(abspath):
                dest = os.path.join(
                    options.profilePath,
                    os.path.basename(abspath))
                shutil.copytree(abspath, dest)
            else:
                self.log.warning(
                    "runtests.py | Failed to copy %s to profile" %
                    abspath)

    def installChromeJar(self, chrome, options):
        """
          copy mochijar directory to profile as an extension so we have chrome://mochikit for all harness code
        """
        # Write chrome.manifest.
        with open(os.path.join(options.profilePath, "extensions", "staged", "mochikit@mozilla.org", "chrome.manifest"), "a") as mfile:
            mfile.write(chrome)

    def getChromeTestDir(self, options):
        dir = os.path.join(os.path.abspath("."), SCRIPT_DIR) + "/"
        if mozinfo.isWin:
            dir = "file:///" + dir.replace("\\", "/")
        return dir

    def writeChromeManifest(self, options):
        manifest = os.path.join(options.profilePath, "tests.manifest")
        with open(manifest, "w") as manifestFile:
            # Register chrome directory.
            chrometestDir = self.getChromeTestDir(options)
            manifestFile.write(
                "content mochitests %s contentaccessible=yes\n" %
                chrometestDir)
            manifestFile.write(
                "content mochitests-any %s contentaccessible=yes remoteenabled=yes\n" %
                chrometestDir)
            manifestFile.write(
                "content mochitests-content %s contentaccessible=yes remoterequired=yes\n" %
                chrometestDir)

            if options.testingModulesDir is not None:
                manifestFile.write("resource testing-common file:///%s\n" %
                                   options.testingModulesDir)
        return manifest

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

        manifest = self.writeChromeManifest(options)

        # Call installChromeJar().
        if not os.path.isdir(os.path.join(SCRIPT_DIR, self.jarDir)):
            self.log.info(
                "TEST-UNEXPECTED-FAIL | invalid setup: missing mochikit extension")
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

        if options.jetpackPackage:
            chrome += """
overlay chrome://browser/content/browser.xul chrome://mochikit/content/jetpack-package-overlay.xul
"""

        if options.jetpackAddon:
            chrome += """
overlay chrome://browser/content/browser.xul chrome://mochikit/content/jetpack-addon-overlay.xul
"""

        self.installChromeJar(chrome, options)
        return manifest

    def getExtensionsToInstall(self, options):
        "Return a list of extensions to install in the profile"
        extensions = options.extensionsToInstall or []
        appDir = options.app[
            :options.app.rfind(
                os.sep)] if options.app else options.utilityPath

        extensionDirs = [
            # Extensions distributed with the test harness.
            os.path.normpath(os.path.join(SCRIPT_DIR, "extensions")),
        ]
        if appDir:
            # Extensions distributed with the application.
            extensionDirs.append(
                os.path.join(
                    appDir,
                    "distribution",
                    "extensions"))

        for extensionDir in extensionDirs:
            if os.path.isdir(extensionDir):
                for dirEntry in os.listdir(extensionDir):
                    if dirEntry not in options.extensionsToExclude:
                        path = os.path.join(extensionDir, dirEntry)
                        if os.path.isdir(path) or (
                                os.path.isfile(path) and path.endswith(".xpi")):
                            extensions.append(path)

        # append mochikit
        extensions.append(os.path.join(SCRIPT_DIR, self.jarDir))
        return extensions


class SSLTunnel:

    def __init__(self, options, logger, ignoreSSLTunnelExts=False):
        self.log = logger
        self.process = None
        self.utilityPath = options.utilityPath
        self.xrePath = options.xrePath
        self.certPath = options.certPath
        self.sslPort = options.sslPort
        self.httpPort = options.httpPort
        self.webServer = options.webServer
        self.webSocketPort = options.webSocketPort
        self.useSSLTunnelExts = not ignoreSSLTunnelExts

        self.customCertRE = re.compile("^cert=(?P<nickname>[0-9a-zA-Z_ ]+)")
        self.clientAuthRE = re.compile("^clientauth=(?P<clientauth>[a-z]+)")
        self.redirRE = re.compile("^redir=(?P<redirhost>[0-9a-zA-Z_ .]+)")

    def writeLocation(self, config, loc):
        for option in loc.options:
            match = self.customCertRE.match(option)
            if match:
                customcert = match.group("nickname")
                config.write("listen:%s:%s:%s:%s\n" %
                             (loc.host, loc.port, self.sslPort, customcert))

            match = self.clientAuthRE.match(option)
            if match:
                clientauth = match.group("clientauth")
                config.write("clientauth:%s:%s:%s:%s\n" %
                             (loc.host, loc.port, self.sslPort, clientauth))

            match = self.redirRE.match(option)
            if match:
                redirhost = match.group("redirhost")
                config.write("redirhost:%s:%s:%s:%s\n" %
                             (loc.host, loc.port, self.sslPort, redirhost))

            if self.useSSLTunnelExts and option in (
                    'ssl3',
                    'rc4',
                    'failHandshake'):
                config.write(
                    "%s:%s:%s:%s\n" %
                    (option, loc.host, loc.port, self.sslPort))

    def buildConfig(self, locations):
        """Create the ssltunnel configuration file"""
        configFd, self.configFile = tempfile.mkstemp(
            prefix="ssltunnel", suffix=".cfg")
        with os.fdopen(configFd, "w") as config:
            config.write("httpproxy:1\n")
            config.write("certdbdir:%s\n" % self.certPath)
            config.write("forward:127.0.0.1:%s\n" % self.httpPort)
            config.write(
                "websocketserver:%s:%s\n" %
                (self.webServer, self.webSocketPort))
            config.write("listen:*:%s:pgo server certificate\n" % self.sslPort)

            for loc in locations:
                if loc.scheme == "https" and "nocert" not in loc.options:
                    self.writeLocation(config, loc)

    def start(self):
        """ Starts the SSL Tunnel """

        # start ssltunnel to provide https:// URLs capability
        bin_suffix = mozinfo.info.get('bin_suffix', '')
        ssltunnel = os.path.join(self.utilityPath, "ssltunnel" + bin_suffix)
        if not os.path.exists(ssltunnel):
            self.log.error(
                "INFO | runtests.py | expected to find ssltunnel at %s" %
                ssltunnel)
            exit(1)

        env = environment(xrePath=self.xrePath)
        env["LD_LIBRARY_PATH"] = self.xrePath
        self.process = mozprocess.ProcessHandler([ssltunnel, self.configFile],
                                                 env=env)
        self.process.run()
        self.log.info("runtests.py | SSL tunnel pid: %d" % self.process.pid)

    def stop(self):
        """ Stops the SSL Tunnel and cleans up """
        if self.process is not None:
            self.process.kill()
        if os.path.exists(self.configFile):
            os.remove(self.configFile)


def checkAndConfigureV4l2loopback(device):
    '''
    Determine if a given device path is a v4l2loopback device, and if so
    toggle a few settings on it via fcntl. Very linux-specific.

    Returns (status, device name) where status is a boolean.
    '''
    if not mozinfo.isLinux:
        return False, ''

    libc = ctypes.cdll.LoadLibrary('libc.so.6')
    O_RDWR = 2
    # These are from linux/videodev2.h

    class v4l2_capability(ctypes.Structure):
        _fields_ = [
            ('driver', ctypes.c_char * 16),
            ('card', ctypes.c_char * 32),
            ('bus_info', ctypes.c_char * 32),
            ('version', ctypes.c_uint32),
            ('capabilities', ctypes.c_uint32),
            ('device_caps', ctypes.c_uint32),
            ('reserved', ctypes.c_uint32 * 3)
        ]
    VIDIOC_QUERYCAP = 0x80685600

    fd = libc.open(device, O_RDWR)
    if fd < 0:
        return False, ''

    vcap = v4l2_capability()
    if libc.ioctl(fd, VIDIOC_QUERYCAP, ctypes.byref(vcap)) != 0:
        return False, ''

    if vcap.driver != 'v4l2 loopback':
        return False, ''

    class v4l2_control(ctypes.Structure):
        _fields_ = [
            ('id', ctypes.c_uint32),
            ('value', ctypes.c_int32)
        ]

    # These are private v4l2 control IDs, see:
    # https://github.com/umlaeute/v4l2loopback/blob/fd822cf0faaccdf5f548cddd9a5a3dcebb6d584d/v4l2loopback.c#L131
    KEEP_FORMAT = 0x8000000
    SUSTAIN_FRAMERATE = 0x8000001
    VIDIOC_S_CTRL = 0xc008561c

    control = v4l2_control()
    control.id = KEEP_FORMAT
    control.value = 1
    libc.ioctl(fd, VIDIOC_S_CTRL, ctypes.byref(control))

    control.id = SUSTAIN_FRAMERATE
    control.value = 1
    libc.ioctl(fd, VIDIOC_S_CTRL, ctypes.byref(control))
    libc.close(fd)

    return True, vcap.card


def findTestMediaDevices(log):
    '''
    Find the test media devices configured on this system, and return a dict
    containing information about them. The dict will have keys for 'audio'
    and 'video', each containing the name of the media device to use.

    If audio and video devices could not be found, return None.

    This method is only currently implemented for Linux.
    '''
    if not mozinfo.isLinux:
        return None

    info = {}
    # Look for a v4l2loopback device.
    name = None
    device = None
    for dev in sorted(glob.glob('/dev/video*')):
        result, name_ = checkAndConfigureV4l2loopback(dev)
        if result:
            name = name_
            device = dev
            break

    if not (name and device):
        log.error('Couldn\'t find a v4l2loopback video device')
        return None

    # Feed it a frame of output so it has something to display
    subprocess.check_call(['/usr/bin/gst-launch-0.10', 'videotestsrc',
                           'pattern=green', 'num-buffers=1', '!',
                           'v4l2sink', 'device=%s' % device])
    info['video'] = name

    # Use pactl to see if the PulseAudio module-sine-source module is loaded.
    def sine_source_loaded():
        o = subprocess.check_output(
            ['/usr/bin/pactl', 'list', 'short', 'modules'])
        return filter(lambda x: 'module-sine-source' in x, o.splitlines())

    if not sine_source_loaded():
        # Load module-sine-source
        subprocess.check_call(['/usr/bin/pactl', 'load-module',
                               'module-sine-source'])
    if not sine_source_loaded():
        log.error('Couldn\'t load module-sine-source')
        return None

    # Hardcode the name since it's always the same.
    info['audio'] = 'Sine source at 440 Hz'
    return info


class KeyValueParseError(Exception):

    """error when parsing strings of serialized key-values"""

    def __init__(self, msg, errors=()):
        self.errors = errors
        Exception.__init__(self, msg)


def parseKeyValue(strings, separator='=', context='key, value: '):
    """
    parse string-serialized key-value pairs in the form of
    `key = value`. Returns a list of 2-tuples.
    Note that whitespace is not stripped.
    """

    # syntax check
    missing = [string for string in strings if separator not in string]
    if missing:
        raise KeyValueParseError(
            "Error: syntax error in %s" %
            (context, ','.join(missing)), errors=missing)
    return [string.split(separator, 1) for string in strings]


class Mochitest(MochitestUtilsMixin):
    _active_tests = None
    certdbNew = False
    sslTunnel = None
    vmwareHelper = None
    DEFAULT_TIMEOUT = 60.0
    mediaDevices = None

    # XXX use automation.py for test name to avoid breaking legacy
    # TODO: replace this with 'runtests.py' or 'mochitest' or the like
    test_name = 'automation.py'

    def __init__(self, logger_options):
        super(Mochitest, self).__init__(logger_options)

        # environment function for browserEnv
        self.environment = environment

        # Max time in seconds to wait for server startup before tests will fail -- if
        # this seems big, it's mostly for debug machines where cold startup
        # (particularly after a build) takes forever.
        self.SERVER_STARTUP_TIMEOUT = 180 if mozinfo.info.get('debug') else 90

        # metro browser sub process id
        self.browserProcessId = None

        self.haveDumpedScreen = False
        # Create variables to count the number of passes, fails, todos.
        self.countpass = 0
        self.countfail = 0
        self.counttodo = 0

        self.expectedError = {}
        self.result = {}

    def extraPrefs(self, extraPrefs):
        """interpolate extra preferences from option strings"""

        try:
            return dict(parseKeyValue(extraPrefs, context='--setpref='))
        except KeyValueParseError as e:
            print str(e)
            sys.exit(1)

    def fillCertificateDB(self, options):
        # TODO: move -> mozprofile:
        # https://bugzilla.mozilla.org/show_bug.cgi?id=746243#c35

        pwfilePath = os.path.join(options.profilePath, ".crtdbpw")
        with open(pwfilePath, "w") as pwfile:
            pwfile.write("\n")

        # Pre-create the certification database for the profile
        env = self.environment(xrePath=options.xrePath)
        env["LD_LIBRARY_PATH"] = options.xrePath
        bin_suffix = mozinfo.info.get('bin_suffix', '')
        certutil = os.path.join(options.utilityPath, "certutil" + bin_suffix)
        pk12util = os.path.join(options.utilityPath, "pk12util" + bin_suffix)
        toolsEnv = env
        if mozinfo.info["asan"]:
            # Disable leak checking when running these tools
            toolsEnv["ASAN_OPTIONS"] = "detect_leaks=0"
        if mozinfo.info["tsan"]:
            # Disable race checking when running these tools
            toolsEnv["TSAN_OPTIONS"] = "report_bugs=0"

        if self.certdbNew:
            # android and b2g use the new DB formats exclusively
            certdbPath = "sql:" + options.profilePath
        else:
            # desktop seems to use the old
            certdbPath = options.profilePath

        status = call(
            [certutil, "-N", "-d", certdbPath, "-f", pwfilePath], env=toolsEnv)
        if status:
            return status

        # Walk the cert directory and add custom CAs and client certs
        files = os.listdir(options.certPath)
        for item in files:
            root, ext = os.path.splitext(item)
            if ext == ".ca":
                trustBits = "CT,,"
                if root.endswith("-object"):
                    trustBits = "CT,,CT"
                call([certutil,
                      "-A",
                      "-i",
                      os.path.join(options.certPath,
                                   item),
                      "-d",
                      certdbPath,
                      "-f",
                      pwfilePath,
                      "-n",
                      root,
                      "-t",
                      trustBits],
                     env=toolsEnv)
            elif ext == ".client":
                call([pk12util, "-i", os.path.join(options.certPath, item),
                      "-w", pwfilePath, "-d", certdbPath],
                     env=toolsEnv)

        os.unlink(pwfilePath)
        return 0

    def buildProfile(self, options):
        """ create the profile and add optional chrome bits and files if requested """
        if options.browserChrome and options.timeout:
            options.extraPrefs.append(
                "testing.browserTestHarness.timeout=%d" %
                options.timeout)
        options.extraPrefs.append(
            "browser.tabs.remote.autostart=%s" %
            ('true' if options.e10s else 'false'))
        if options.strictContentSandbox:
            options.extraPrefs.append("security.sandbox.content.level=1")
        options.extraPrefs.append(
            "dom.ipc.tabs.nested.enabled=%s" %
            ('true' if options.nested_oop else 'false'))

        # get extensions to install
        extensions = self.getExtensionsToInstall(options)

        # web apps
        appsPath = os.path.join(
            SCRIPT_DIR,
            'profile_data',
            'webapps_mochitest.json')
        if os.path.exists(appsPath):
            with open(appsPath) as apps_file:
                apps = json.load(apps_file)
        else:
            apps = None

        # preferences
        preferences = [
            os.path.join(
                SCRIPT_DIR,
                'profile_data',
                'prefs_general.js')]

        # TODO: Let's include those prefs until bug 1072443 is fixed
        if mozinfo.info.get('buildapp') == 'mulet':
            preferences += [os.path.join(SCRIPT_DIR,
                                         'profile_data',
                                         'prefs_b2g_unittest.js')]

        prefs = {}
        for path in preferences:
            prefs.update(Preferences.read_prefs(path))

        prefs.update(self.extraPrefs(options.extraPrefs))

        # interpolate preferences
        interpolation = {
            "server": "%s:%s" %
            (options.webServer, options.httpPort)}

        # TODO: Remove OOP once bug 1072443 is fixed
        if mozinfo.info.get('buildapp') == 'mulet':
            interpolation["OOP"] = "false"

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

        # See if we should use fake media devices.
        if options.useTestMediaDevices:
            prefs['media.audio_loopback_dev'] = self.mediaDevices['audio']
            prefs['media.video_loopback_dev'] = self.mediaDevices['video']

        # create a profile
        self.profile = Profile(profile=options.profilePath,
                               addons=extensions,
                               locations=self.locations,
                               preferences=prefs,
                               apps=apps,
                               proxy=proxy
                               )

        # Fix options.profilePath for legacy consumers.
        options.profilePath = self.profile.profile

        manifest = self.addChromeToProfile(options)
        self.copyExtraFilesToProfile(options)

        # create certificate database for the profile
        # TODO: this should really be upstreamed somewhere, maybe mozprofile
        certificateStatus = self.fillCertificateDB(options)
        if certificateStatus:
            self.log.info(
                "TEST-UNEXPECTED-FAIL | runtests.py | Certificate integration failed")
            return None

        return manifest

    def getGMPPluginPath(self, options):
        if options.gmp_path:
            return options.gmp_path

        gmp_parentdirs = [
            # For local builds, GMP plugins will be under dist/bin.
            options.xrePath,
            # For packaged builds, GMP plugins will get copied under
            # $profile/plugins.
            os.path.join(self.profile.profile, 'plugins'),
        ]

        gmp_subdirs = [
            os.path.join('gmp-fake', '1.0'),
            os.path.join('gmp-fakeopenh264', '1.0'),
            os.path.join('gmp-clearkey', '0.1'),
        ]

        gmp_paths = [os.path.join(parent, sub)
                     for parent in gmp_parentdirs
                     for sub in gmp_subdirs
                     if os.path.isdir(os.path.join(parent, sub))]

        if not gmp_paths:
            # This is fatal for desktop environments.
            raise EnvironmentError('Could not find test gmp plugins')

        return os.pathsep.join(gmp_paths)

    def buildBrowserEnv(self, options, debugger=False, env=None):
        """build the environment variables for the specific test and operating system"""
        if mozinfo.info["asan"]:
            lsanPath = SCRIPT_DIR
        else:
            lsanPath = None

        browserEnv = self.environment(
            xrePath=options.xrePath,
            env=env,
            debugger=debugger,
            dmdPath=options.dmdPath,
            lsanPath=lsanPath)

        # These variables are necessary for correct application startup; change
        # via the commandline at your own risk.
        browserEnv["XPCOM_DEBUG_BREAK"] = "stack"

        # When creating child processes on Windows pre-Vista (e.g. Windows XP) we
        # don't normally inherit stdout/err handles, because you can only do it by
        # inheriting all other inheritable handles as well.
        # We need to inherit them for plain mochitests for test logging purposes, so
        # we do so on the basis of a specific environment variable.
        if self.getTestFlavor(options) == "mochitest":
            browserEnv["MOZ_WIN_INHERIT_STD_HANDLES_PRE_VISTA"] = "1"

        # interpolate environment passed with options
        try:
            browserEnv.update(
                dict(
                    parseKeyValue(
                        options.environment,
                        context='--setenv')))
        except KeyValueParseError as e:
            self.log.error(str(e))
            return None

        browserEnv["XPCOM_MEM_BLOAT_LOG"] = self.leak_report_file

        try:
            gmp_path = self.getGMPPluginPath(options)
            if gmp_path is not None:
                browserEnv["MOZ_GMP_PATH"] = gmp_path
        except EnvironmentError:
            self.log.error('Could not find path to gmp-fake plugin!')
            return None

        if options.fatalAssertions:
            browserEnv["XPCOM_DEBUG_BREAK"] = "stack-and-abort"

        # Produce an NSPR log, is setup (see NSPR_LOG_MODULES global at the top of
        # this script).
        self.nsprLogs = NSPR_LOG_MODULES and "MOZ_UPLOAD_DIR" in os.environ
        if self.nsprLogs:
            browserEnv["NSPR_LOG_MODULES"] = NSPR_LOG_MODULES

            browserEnv["NSPR_LOG_FILE"] = "%s/nspr.log" % tempfile.gettempdir()
            browserEnv["GECKO_SEPARATE_NSPR_LOGS"] = "1"

        if debugger and not options.slowscript:
            browserEnv["JS_DISABLE_SLOW_SCRIPT_SIGNALS"] = "1"

        # For e10s, our tests default to suppressing the "unsafe CPOW usage"
        # warnings that can plague test logs.
        if not options.enableCPOWWarnings:
            browserEnv["DISABLE_UNSAFE_CPOW_WARNINGS"] = "1"

        return browserEnv

    def cleanup(self, options):
        """ remove temporary files and profile """
        if hasattr(self, 'manifest') and self.manifest is not None:
            os.remove(self.manifest)
        if hasattr(self, 'profile'):
            del self.profile
        if options.pidFile != "":
            try:
                os.remove(options.pidFile)
                if os.path.exists(options.pidFile + ".xpcshell.pid"):
                    os.remove(options.pidFile + ".xpcshell.pid")
            except:
                self.log.warning(
                    "cleaning up pidfile '%s' was unsuccessful from the test harness" %
                    options.pidFile)
        options.manifestFile = None

    def dumpScreen(self, utilityPath):
        if self.haveDumpedScreen:
            self.log.info(
                "Not taking screenshot here: see the one that was previously logged")
            return
        self.haveDumpedScreen = True
        dumpScreen(utilityPath)

    def killAndGetStack(
            self,
            processPID,
            utilityPath,
            debuggerInfo,
            dump_screen=False):
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
                crashinject = os.path.normpath(
                    os.path.join(
                        utilityPath,
                        "crashinject.exe"))
                if os.path.exists(crashinject):
                    status = subprocess.Popen(
                        [crashinject, str(processPID)]).wait()
                    printstatus(status, "crashinject")
                    if status == 0:
                        return
            else:
                try:
                    os.kill(processPID, signal.SIGABRT)
                except OSError:
                    # https://bugzilla.mozilla.org/show_bug.cgi?id=921509
                    self.log.info(
                        "Can't trigger Breakpad, process no longer exists")
                return
        self.log.info("Can't trigger Breakpad, just killing process")
        killPid(processPID, self.log)

    def checkForZombies(self, processLog, utilityPath, debuggerInfo):
        """Look for hung processes"""

        if not os.path.exists(processLog):
            self.log.info(
                'Automation Error: PID log not found: %s' %
                processLog)
            # Whilst no hung process was found, the run should still display as
            # a failure
            return True

        # scan processLog for zombies
        self.log.info('zombiecheck | Reading PID log: %s' % processLog)
        processList = []
        pidRE = re.compile(r'launched child process (\d+)$')
        with open(processLog) as processLogFD:
            for line in processLogFD:
                self.log.info(line.rstrip())
                m = pidRE.search(line)
                if m:
                    processList.append(int(m.group(1)))

        # kill zombies
        foundZombie = False
        for processPID in processList:
            self.log.info(
                "zombiecheck | Checking for orphan process with PID: %d" %
                processPID)
            if isPidAlive(processPID):
                foundZombie = True
                self.log.info(
                    "TEST-UNEXPECTED-FAIL | zombiecheck | child process %d still alive after shutdown" %
                    processPID)
                self.killAndGetStack(
                    processPID,
                    utilityPath,
                    debuggerInfo,
                    dump_screen=not debuggerInfo)

        return foundZombie

    def startVMwareRecording(self, options):
        """ starts recording inside VMware VM using the recording helper dll """
        assert mozinfo.isWin
        from ctypes import cdll
        self.vmwareHelper = cdll.LoadLibrary(self.vmwareHelperPath)
        if self.vmwareHelper is None:
            self.log.warning("runtests.py | Failed to load "
                             "VMware recording helper")
            return
        self.log.info("runtests.py | Starting VMware recording.")
        try:
            self.vmwareHelper.StartRecording()
        except Exception as e:
            self.log.warning("runtests.py | Failed to start "
                             "VMware recording: (%s)" % str(e))
            self.vmwareHelper = None

    def stopVMwareRecording(self):
        """ stops recording inside VMware VM using the recording helper dll """
        try:
            assert mozinfo.isWin
            if self.vmwareHelper is not None:
                self.log.info("runtests.py | Stopping VMware recording.")
                self.vmwareHelper.StopRecording()
        except Exception as e:
            self.log.warning("runtests.py | Failed to stop "
                             "VMware recording: (%s)" % str(e))
            self.log.exception('Error stopping VMWare recording')

        self.vmwareHelper = None

    def runApp(self,
               testUrl,
               env,
               app,
               profile,
               extraArgs,
               utilityPath,
               debuggerInfo=None,
               symbolsPath=None,
               timeout=-1,
               onLaunch=None,
               detectShutdownLeaks=False,
               screenshotOnFail=False,
               testPath=None,
               bisectChunk=None,
               quiet=False):
        """
        Run the app, log the duration it took to execute, return the status code.
        Kills the app if it runs for longer than |maxTime| seconds, or outputs nothing for |timeout| seconds.
        """

        # configure the message logger buffering
        self.message_logger.buffering = quiet

        # debugger information
        interactive = False
        debug_args = None
        if debuggerInfo:
            interactive = debuggerInfo.interactive
            debug_args = [debuggerInfo.path] + debuggerInfo.args

        # fix default timeout
        if timeout == -1:
            timeout = self.DEFAULT_TIMEOUT

        # copy env so we don't munge the caller's environment
        env = env.copy()

        # make sure we clean up after ourselves.
        try:
            # set process log environment variable
            tmpfd, processLog = tempfile.mkstemp(suffix='pidlog')
            os.close(tmpfd)
            env["MOZ_PROCESS_LOG"] = processLog

            if debuggerInfo:
                # If a debugger is attached, don't use timeouts, and don't
                # capture ctrl-c.
                timeout = None
                signal.signal(signal.SIGINT, lambda sigid, frame: None)

            # build command line
            cmd = os.path.abspath(app)
            args = list(extraArgs)
            # TODO: mozrunner should use -foreground at least for mac
            # https://bugzilla.mozilla.org/show_bug.cgi?id=916512
            args.append('-foreground')
            if testUrl:
                if debuggerInfo and debuggerInfo.requiresEscapedArgs:
                    testUrl = testUrl.replace("&", "\\&")
                args.append(testUrl)

            if detectShutdownLeaks:
                shutdownLeaks = ShutdownLeaks(self.log)
            else:
                shutdownLeaks = None

            if mozinfo.info["asan"] and (mozinfo.isLinux or mozinfo.isMac):
                lsanLeaks = LSANLeaks(self.log)
            else:
                lsanLeaks = None

            # create an instance to process the output
            outputHandler = self.OutputHandler(
                harness=self,
                utilityPath=utilityPath,
                symbolsPath=symbolsPath,
                dump_screen_on_timeout=not debuggerInfo,
                dump_screen_on_fail=screenshotOnFail,
                shutdownLeaks=shutdownLeaks,
                lsanLeaks=lsanLeaks,
                bisectChunk=bisectChunk)

            def timeoutHandler():
                browserProcessId = outputHandler.browserProcessId
                self.handleTimeout(
                    timeout,
                    proc,
                    utilityPath,
                    debuggerInfo,
                    browserProcessId,
                    testPath)
            kp_kwargs = {'kill_on_timeout': False,
                         'cwd': SCRIPT_DIR,
                         'onTimeout': [timeoutHandler]}
            kp_kwargs['processOutputLine'] = [outputHandler]

            # create mozrunner instance and start the system under test process
            self.lastTestSeen = self.test_name
            startTime = datetime.now()

            # b2g desktop requires Runner even though appname is b2g
            if mozinfo.info.get('appname') == 'b2g' and mozinfo.info.get(
                    'toolkit') != 'gonk':
                runner_cls = mozrunner.Runner
            else:
                runner_cls = mozrunner.runners.get(
                    mozinfo.info.get(
                        'appname',
                        'firefox'),
                    mozrunner.Runner)
            runner = runner_cls(profile=self.profile,
                                binary=cmd,
                                cmdargs=args,
                                env=env,
                                process_class=mozprocess.ProcessHandlerMixin,
                                process_args=kp_kwargs)

            # start the runner
            runner.start(debug_args=debug_args,
                         interactive=interactive,
                         outputTimeout=timeout)
            proc = runner.process_handler
            self.log.info("runtests.py | Application pid: %d" % proc.pid)

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
            printstatus(status, "Main app process")
            runner.process_handler = None

            # finalize output handler
            outputHandler.finish()

            # record post-test information
            if status:
                self.message_logger.dump_buffered()
                self.log.info(
                    "TEST-UNEXPECTED-FAIL | %s | application terminated with exit code %s" %
                    (self.lastTestSeen, status))
            else:
                self.lastTestSeen = 'Main app process exited normally'

            self.log.info(
                "runtests.py | Application ran for: %s" % str(
                    datetime.now() - startTime))

            # Do a final check for zombie child processes.
            zombieProcesses = self.checkForZombies(
                processLog,
                utilityPath,
                debuggerInfo)

            # check for crashes
            minidump_path = os.path.join(self.profile.profile, "minidumps")
            crash_count = mozcrash.log_crashes(
                self.log,
                minidump_path,
                symbolsPath,
                test=self.lastTestSeen)

            if crash_count or zombieProcesses:
                status = 1

        finally:
            # cleanup
            if os.path.exists(processLog):
                os.remove(processLog)

        return status

    def initializeLooping(self, options):
        """
          This method is used to clear the contents before each run of for loop.
          This method is used for --run-by-dir and --bisect-chunk.
        """
        self.expectedError.clear()
        self.result.clear()
        options.manifestFile = None
        options.profilePath = None
        self.urlOpts = []

    def resolve_runtime_file(self, options, info):
        platform = info['platform_guess']
        buildtype = info['buildtype_guess']

        data_dir = os.path.join(SCRIPT_DIR, 'runtimes', '{}-{}'.format(
            platform, buildtype))

        flavor = self.getTestFlavor(options)
        if flavor == 'browser-chrome' and options.subsuite == 'devtools':
            flavor = 'devtools-chrome'
        elif flavor == 'mochitest':
            flavor = 'plain'

        base = 'mochitest'
        if options.e10s:
            base = '{}-e10s'.format(base)
        return os.path.join(data_dir, '{}-{}.runtimes.json'.format(
            base, flavor))


    def getActiveTests(self, options, disabled=True):
        """
          This method is used to parse the manifest and return active filtered tests.
        """
        if self._active_tests:
            return self._active_tests

        self.setTestRoot(options)
        manifest = self.getTestManifest(options)
        if manifest:
            info = mozinfo.info

            # Bug 883858 - return all tests including disabled tests
            testPath = self.getTestPath(options)
            testPath = testPath.replace('\\', '/')
            if testPath.endswith('.html') or \
               testPath.endswith('.xhtml') or \
               testPath.endswith('.xul') or \
               testPath.endswith('.js'):
                # In the case where we have a single file, we don't want to
                # filter based on options such as subsuite.
                tests = manifest.active_tests(
                    exists=False, disabled=disabled, **info)
                for test in tests:
                    if 'disabled' in test:
                        del test['disabled']

            else:
                # Bug 1089034 - imptest failure expectations are encoded as
                # test manifests, even though they aren't tests. This gross
                # hack causes several problems in automation including
                # throwing off the chunking numbers. Remove them manually
                # until bug 1089034 is fixed.
                def remove_imptest_failure_expectations(tests, values):
                    return (t for t in tests
                            if 'imptests/failures' not in t['path'])

                filters = [
                    remove_imptest_failure_expectations,
                    subsuite(options.subsuite),
                ]

                # Add chunking filters if specified
                if options.totalChunks:
                    if options.chunkByDir:
                        filters.append(chunk_by_dir(options.thisChunk,
                                                    options.totalChunks,
                                                    options.chunkByDir))
                    elif options.chunkByRuntime:
                        runtime_file = self.resolve_runtime_file(options, info)
                        with open(runtime_file, 'r') as f:
                            runtime_data = json.loads(f.read())
                        runtimes = runtime_data['runtimes']
                        default = runtime_data['excluded_test_average']
                        filters.append(
                            chunk_by_runtime(options.thisChunk,
                                             options.totalChunks,
                                             runtimes,
                                             default_runtime=default))
                    else:
                        filters.append(chunk_by_slice(options.thisChunk,
                                                      options.totalChunks))

                if options.test_tags:
                    filters.append(tags(options.test_tags))

                tests = manifest.active_tests(
                    exists=False, disabled=disabled, filters=filters, **info)

                if len(tests) == 0:
                    self.log.error("no tests to run using specified "
                                   "combination of filters: {}".format(
                                        manifest.fmt_filters()))

        paths = []

        for test in tests:
            if len(tests) == 1 and 'disabled' in test:
                del test['disabled']

            pathAbs = os.path.abspath(test['path'])
            assert pathAbs.startswith(self.testRootAbs)
            tp = pathAbs[len(self.testRootAbs):].replace('\\', '/').strip('/')

            # Filter out tests if we are using --test-path
            if testPath and not tp.startswith(testPath):
                continue

            if not self.isTest(options, tp):
                self.log.warning(
                    'Warning: %s from manifest %s is not a valid test' %
                    (test['name'], test['manifest']))
                continue

            testob = {'path': tp}
            if 'disabled' in test:
                testob['disabled'] = test['disabled']
            if 'expected' in test:
                testob['expected'] = test['expected']
            paths.append(testob)

        def path_sort(ob1, ob2):
            path1 = ob1['path'].split('/')
            path2 = ob2['path'].split('/')
            return cmp(path1, path2)

        paths.sort(path_sort)
        self._active_tests = paths
        return self._active_tests

    def logPreamble(self, tests):
        """Logs a suite_start message and test_start/test_end at the beginning of a run.
        """
        self.log.suite_start([t['path'] for t in tests])
        for test in tests:
            if 'disabled' in test:
                self.log.test_start(test['path'])
                self.log.test_end(
                    test['path'],
                    'SKIP',
                    message=test['disabled'])

    def getTestsToRun(self, options):
        """
          This method makes a list of tests that are to be run. Required mainly for --bisect-chunk.
        """
        tests = self.getActiveTests(options)
        self.logPreamble(tests)

        testsToRun = []
        for test in tests:
            if 'disabled' in test:
                continue
            testsToRun.append(test['path'])

        return testsToRun

    def runMochitests(self, options, testsToRun, onLaunch=None):
        "This is a base method for calling other methods in this class for --bisect-chunk."
        # Making an instance of bisect class for --bisect-chunk option.
        bisect = bisection.Bisect(self)
        finished = False
        status = 0
        bisection_log = 0
        while not finished:
            if options.bisectChunk:
                testsToRun = bisect.pre_test(options, testsToRun, status)
                # To inform that we are in the process of bisection, and to
                # look for bleedthrough
                if options.bisectChunk != "default" and not bisection_log:
                    self.log.info(
                        "TEST-UNEXPECTED-FAIL | Bisection | Please ignore repeats and look for 'Bleedthrough' (if any) at the end of the failure list")
                    bisection_log = 1

            result = self.doTests(options, onLaunch, testsToRun)
            if options.bisectChunk:
                status = bisect.post_test(
                    options,
                    self.expectedError,
                    self.result)
            else:
                status = -1

            if status == -1:
                finished = True

        # We need to print the summary only if options.bisectChunk has a value.
        # Also we need to make sure that we do not print the summary in between
        # running tests via --run-by-dir.
        if options.bisectChunk and options.bisectChunk in self.result:
            bisect.print_summary()

        return result

    def killNamedOrphans(self, pname):
        """ Kill orphan processes matching the given command name """
        self.log.info("Checking for orphan %s processes..." % pname)
        def _psInfo(line):
            if pname in line:
                self.log.info(line)
        process = mozprocess.ProcessHandler(['ps', '-f'],
                                            processOutputLine=_psInfo)
        process.run()
        process.wait()

        def _psKill(line):
            parts = line.split()
            if len(parts) == 3 and parts[0].isdigit():
                pid = int(parts[0])
                if parts[2] == pname and parts[1] == '1':
                    self.log.info("killing %s orphan with pid %d" % (pname, pid))
                    killPid(pid, self.log)
        process = mozprocess.ProcessHandler(['ps', '-o', 'pid,ppid,comm'],
                                            processOutputLine=_psKill)
        process.run()
        process.wait()

    def runTests(self, options, onLaunch=None):
        """ Prepare, configure, run tests and cleanup """

        self.setTestRoot(options)

        # Despite our efforts to clean up servers started by this script, in practice
        # we still see infrequent cases where a process is orphaned and interferes
        # with future tests, typically because the old server is keeping the port in use.
        # Try to avoid those failures by checking for and killing orphan servers before
        # trying to start new ones.
        self.killNamedOrphans('ssltunnel')
        self.killNamedOrphans('xpcshell')

        # Until we have all green, this only runs on bc*/dt*/mochitest-chrome
        # jobs, not webapprt*, jetpack*, or plain
        if options.browserChrome:
            options.runByDir = True

        testsToRun = self.getTestsToRun(options)
        if not options.runByDir:
            return self.runMochitests(options, testsToRun, onLaunch)

        # code for --run-by-dir
        dirs = self.getDirectories(options)

        result = 1  # default value, if no tests are run.
        inputTestPath = self.getTestPath(options)
        for d in dirs:
            if inputTestPath and not inputTestPath.startswith(d):
                continue

            print "dir: %s" % d
            tests_in_dir = [t for t in testsToRun if os.path.dirname(t) == d]

            # If we are using --run-by-dir, we should not use the profile path (if) provided
            # by the user, since we need to create a new directory for each run. We would face problems
            # if we use the directory provided by the user.
            result = self.runMochitests(options, tests_in_dir, onLaunch)

            # Dump the logging buffer
            self.message_logger.dump_buffered()

            if result == -1:
                break

        # printing total number of tests
        if options.browserChrome:
            print "TEST-INFO | checking window state"
            print "Browser Chrome Test Summary"
            print "\tPassed: %s" % self.countpass
            print "\tFailed: %s" % self.countfail
            print "\tTodo: %s" % self.counttodo
            print "*** End BrowserChrome Test Results ***"
        else:
            print "0 INFO TEST-START | Shutdown"
            print "1 INFO Passed:  %s" % self.countpass
            print "2 INFO Failed:  %s" % self.countfail
            print "3 INFO Todo:    %s" % self.counttodo
            print "4 INFO SimpleTest FINISHED"

        return result

    def doTests(self, options, onLaunch=None, testsToFilter=None):
        # A call to initializeLooping method is required in case of --run-by-dir or --bisect-chunk
        # since we need to initialize variables for each loop.
        if options.bisectChunk or options.runByDir:
            self.initializeLooping(options)

        # get debugger info, a dict of:
        # {'path': path to the debugger (string),
        #  'interactive': whether the debugger is interactive or not (bool)
        #  'args': arguments to the debugger (list)
        # TODO: use mozrunner.local.debugger_arguments:
        # https://github.com/mozilla/mozbase/blob/master/mozrunner/mozrunner/local.py#L42

        debuggerInfo = None
        if options.debugger:
            debuggerInfo = mozdebug.get_debugger_info(
                options.debugger,
                options.debuggerArgs,
                options.debuggerInteractive)

        if options.useTestMediaDevices:
            devices = findTestMediaDevices(self.log)
            if not devices:
                self.log.error("Could not find test media devices to use")
                return 1
            self.mediaDevices = devices

        # buildProfile sets self.profile .
        # This relies on sideeffects and isn't very stateful:
        # https://bugzilla.mozilla.org/show_bug.cgi?id=919300
        self.manifest = self.buildProfile(options)
        if self.manifest is None:
            return 1

        self.leak_report_file = os.path.join(
            options.profilePath,
            "runtests_leaks.log")

        self.browserEnv = self.buildBrowserEnv(
            options,
            debuggerInfo is not None)


        # If there are any Mulet-specific tests doing remote network access,
        # we will not be aware since we are explicitely allowing this, as for
        # B2G
        #
        # In addition, the push subsuite directly accesses the production
        # push service.
        if 'MOZ_DISABLE_NONLOCAL_CONNECTIONS' in self.browserEnv:
            if mozinfo.info.get('buildapp') == 'mulet' or options.subsuite == 'push':
                del self.browserEnv['MOZ_DISABLE_NONLOCAL_CONNECTIONS']
                os.environ["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "0"

        if self.browserEnv is None:
            return 1

        try:
            self.startServers(options, debuggerInfo)

            # testsToFilter parameter is used to filter out the test list that
            # is sent to buildTestPath
            testURL = self.buildTestPath(options, testsToFilter)

            # read the number of tests here, if we are not going to run any,
            # terminate early
            if os.path.exists(
                os.path.join(
                    SCRIPT_DIR,
                    options.testRunManifestFile)):
                with open(os.path.join(SCRIPT_DIR, options.testRunManifestFile)) as fHandle:
                    tests = json.load(fHandle)
                count = 0
                for test in tests['tests']:
                    count += 1
                if count == 0:
                    return 1

            self.buildURLOptions(options, self.browserEnv)
            if self.urlOpts:
                testURL += "?" + "&".join(self.urlOpts)

            if options.webapprtContent:
                options.browserArgs.extend(('-test-mode', testURL))
                testURL = None

            if options.immersiveMode:
                options.browserArgs.extend(('-firefoxpath', options.app))
                options.app = self.immersiveHelperPath

            if options.jsdebugger:
                options.browserArgs.extend(['-jsdebugger'])

            # Remove the leak detection file so it can't "leak" to the tests run.
            # The file is not there if leak logging was not enabled in the
            # application build.
            if os.path.exists(self.leak_report_file):
                os.remove(self.leak_report_file)

            # then again to actually run mochitest
            if options.timeout:
                timeout = options.timeout + 30
            elif options.debugger or not options.autorun:
                timeout = None
            else:
                timeout = 330.0  # default JS harness timeout is 300 seconds

            if options.vmwareRecording:
                self.startVMwareRecording(options)

            # detect shutdown leaks for m-bc runs
            detectShutdownLeaks = mozinfo.info[
                "debug"] and options.browserChrome and not options.webapprtChrome

            self.log.info("runtests.py | Running tests: start.\n")
            try:
                status = self.runApp(testURL,
                                     self.browserEnv,
                                     options.app,
                                     profile=self.profile,
                                     extraArgs=options.browserArgs,
                                     utilityPath=options.utilityPath,
                                     debuggerInfo=debuggerInfo,
                                     symbolsPath=options.symbolsPath,
                                     timeout=timeout,
                                     onLaunch=onLaunch,
                                     detectShutdownLeaks=detectShutdownLeaks,
                                     screenshotOnFail=options.screenshotOnFail,
                                     testPath=options.testPath,
                                     bisectChunk=options.bisectChunk,
                                     quiet=options.quiet
                                     )
            except KeyboardInterrupt:
                self.log.info("runtests.py | Received keyboard interrupt.\n")
                status = -1
            except:
                traceback.print_exc()
                self.log.error(
                    "Automation Error: Received unexpected exception while running application\n")
                status = 1

        finally:
            if options.vmwareRecording:
                self.stopVMwareRecording()
            self.stopServers()

        processLeakLog(self.leak_report_file, options)

        if self.nsprLogs:
            with zipfile.ZipFile("%s/nsprlog.zip" % self.browserEnv["MOZ_UPLOAD_DIR"], "w", zipfile.ZIP_DEFLATED) as logzip:
                for logfile in glob.glob(
                        "%s/nspr*.log*" %
                        tempfile.gettempdir()):
                    logzip.write(logfile)
                    os.remove(logfile)

        self.log.info("runtests.py | Running tests: end.")

        if self.manifest is not None:
            self.cleanup(options)

        return status

    def handleTimeout(
            self,
            timeout,
            proc,
            utilityPath,
            debuggerInfo,
            browserProcessId,
            testPath=None):
        """handle process output timeout"""
        # TODO: bug 913975 : _processOutput should call self.processOutputLine
        # one more time one timeout (I think)
        if testPath:
            error_message = "TEST-UNEXPECTED-TIMEOUT | %s | application timed out after %d seconds with no output on %s" % (
                self.lastTestSeen, int(timeout), testPath)
        else:
            error_message = "TEST-UNEXPECTED-TIMEOUT | %s | application timed out after %d seconds with no output" % (
                self.lastTestSeen, int(timeout))

        self.message_logger.dump_buffered()
        self.message_logger.buffering = False
        self.log.info(error_message)

        browserProcessId = browserProcessId or proc.pid
        self.killAndGetStack(
            browserProcessId,
            utilityPath,
            debuggerInfo,
            dump_screen=not debuggerInfo)

    class OutputHandler(object):

        """line output handler for mozrunner"""

        def __init__(
                self,
                harness,
                utilityPath,
                symbolsPath=None,
                dump_screen_on_timeout=True,
                dump_screen_on_fail=False,
                shutdownLeaks=None,
                lsanLeaks=None,
                bisectChunk=None):
            """
            harness -- harness instance
            dump_screen_on_timeout -- whether to dump the screen on timeout
            """
            self.harness = harness
            self.utilityPath = utilityPath
            self.symbolsPath = symbolsPath
            self.dump_screen_on_timeout = dump_screen_on_timeout
            self.dump_screen_on_fail = dump_screen_on_fail
            self.shutdownLeaks = shutdownLeaks
            self.lsanLeaks = lsanLeaks
            self.bisectChunk = bisectChunk

            # With metro browser runs this script launches the metro test harness which launches the browser.
            # The metro test harness hands back the real browser process id via log output which we need to
            # pick up on and parse out. This variable tracks the real browser
            # process id if we find it.
            self.browserProcessId = None

            self.stackFixerFunction = self.stackFixer()

        def processOutputLine(self, line):
            """per line handler of output for mozprocess"""
            # Parsing the line (by the structured messages logger).
            messages = self.harness.message_logger.parse_line(line)

            for message in messages:
                    # Passing the message to the handlers
                for handler in self.outputHandlers():
                    message = handler(message)

                # Processing the message by the logger
                self.harness.message_logger.process_message(message)

        __call__ = processOutputLine

        def outputHandlers(self):
            """returns ordered list of output handlers"""
            handlers = [self.fix_stack,
                        self.record_last_test,
                        self.dumpScreenOnTimeout,
                        self.dumpScreenOnFail,
                        self.trackShutdownLeaks,
                        self.trackLSANLeaks,
                        self.countline,
                        ]
            if self.bisectChunk:
                handlers.append(self.record_result)
                handlers.append(self.first_error)

            return handlers

        def stackFixer(self):
            """
            return stackFixerFunction, if any, to use on the output lines
            """

            if not mozinfo.info.get('debug'):
                return None

            stackFixerFunction = None

            def import_stackFixerModule(module_name):
                sys.path.insert(0, self.utilityPath)
                module = __import__(module_name, globals(), locals(), [])
                sys.path.pop(0)
                return module

            if self.symbolsPath and os.path.exists(self.symbolsPath):
                # Run each line through a function in fix_stack_using_bpsyms.py (uses breakpad symbol files).
                # This method is preferred for Tinderbox builds, since native
                # symbols may have been stripped.
                stackFixerModule = import_stackFixerModule(
                    'fix_stack_using_bpsyms')
                stackFixerFunction = lambda line: stackFixerModule.fixSymbols(
                    line,
                    self.symbolsPath)

            elif mozinfo.isMac:
                # Run each line through fix_macosx_stack.py (uses atos).
                # This method is preferred for developer machines, so we don't
                # have to run "make buildsymbols".
                stackFixerModule = import_stackFixerModule('fix_macosx_stack')
                stackFixerFunction = lambda line: stackFixerModule.fixSymbols(
                    line)

            elif mozinfo.isLinux:
                # Run each line through fix_linux_stack.py (uses addr2line).
                # This method is preferred for developer machines, so we don't
                # have to run "make buildsymbols".
                stackFixerModule = import_stackFixerModule('fix_linux_stack')
                stackFixerFunction = lambda line: stackFixerModule.fixSymbols(
                    line)

            return stackFixerFunction

        def finish(self):
            if self.shutdownLeaks:
                self.shutdownLeaks.process()

            if self.lsanLeaks:
                self.lsanLeaks.process()

        # output message handlers:
        # these take a message and return a message

        def record_result(self, message):
            # by default make the result key equal to pass.
            if message['action'] == 'test_start':
                key = message['test'].split('/')[-1].strip()
                self.harness.result[key] = "PASS"
            elif message['action'] == 'test_status':
                if 'expected' in message:
                    key = message['test'].split('/')[-1].strip()
                    self.harness.result[key] = "FAIL"
                elif message['status'] == 'FAIL':
                    key = message['test'].split('/')[-1].strip()
                    self.harness.result[key] = "TODO"
            return message

        def first_error(self, message):
            if message['action'] == 'test_status' and 'expected' in message and message[
                    'status'] == 'FAIL':
                key = message['test'].split('/')[-1].strip()
                if key not in self.harness.expectedError:
                    self.harness.expectedError[key] = message.get(
                        'message',
                        message['subtest']).strip()
            return message

        def countline(self, message):
            if message['action'] != 'log':
                return message
            line = message['message']
            val = 0
            try:
                val = int(line.split(':')[-1].strip())
            except ValueError:
                return message

            if "Passed:" in line:
                self.harness.countpass += val
            elif "Failed:" in line:
                self.harness.countfail += val
            elif "Todo:" in line:
                self.harness.counttodo += val
            return message

        def fix_stack(self, message):
            if message['action'] == 'log' and self.stackFixerFunction:
                message['message'] = self.stackFixerFunction(
                    message['message'].encode(
                        'utf-8',
                        'replace'))
            return message

        def record_last_test(self, message):
            """record last test on harness"""
            if message['action'] == 'test_start':
                self.harness.lastTestSeen = message['test']
            return message

        def dumpScreenOnTimeout(self, message):
            if (not self.dump_screen_on_fail
                    and self.dump_screen_on_timeout
                    and message['action'] == 'test_status' and 'expected' in message
                    and "Test timed out" in message['subtest']):
                self.harness.dumpScreen(self.utilityPath)
            return message

        def dumpScreenOnFail(self, message):
            if self.dump_screen_on_fail and 'expected' in message and message[
                    'status'] == 'FAIL':
                self.harness.dumpScreen(self.utilityPath)
            return message

        def trackLSANLeaks(self, message):
            if self.lsanLeaks and message['action'] == 'log':
                self.lsanLeaks.log(message['message'])
            return message

        def trackShutdownLeaks(self, message):
            if self.shutdownLeaks:
                self.shutdownLeaks.log(message)
            return message

    def makeTestConfig(self, options):
        "Creates a test configuration file for customizing test execution."
        options.logFile = options.logFile.replace("\\", "\\\\")
        options.testPath = options.testPath.replace("\\", "\\\\")

        if "MOZ_HIDE_RESULTS_TABLE" in os.environ and os.environ[
                "MOZ_HIDE_RESULTS_TABLE"] == "1":
            options.hideResultsTable = True

        d = dict((k, v) for k, v in options.__dict__.items() if (not k.startswith(
            'log_') or not any([k.endswith(fmt) for fmt in commandline.log_formatters.keys()])))
        d['testRoot'] = self.testRoot
        content = json.dumps(d)

        with open(os.path.join(options.profilePath, "testConfig.js"), "w") as config:
            config.write(content)

    def getTestManifest(self, options):
        if isinstance(options.manifestFile, TestManifest):
            manifest = options.manifestFile
        elif options.manifestFile and os.path.isfile(options.manifestFile):
            manifestFileAbs = os.path.abspath(options.manifestFile)
            assert manifestFileAbs.startswith(SCRIPT_DIR)
            manifest = TestManifest([options.manifestFile], strict=False)
        elif options.manifestFile and os.path.isfile(os.path.join(SCRIPT_DIR, options.manifestFile)):
            manifestFileAbs = os.path.abspath(
                os.path.join(
                    SCRIPT_DIR,
                    options.manifestFile))
            assert manifestFileAbs.startswith(SCRIPT_DIR)
            manifest = TestManifest([manifestFileAbs], strict=False)
        else:
            masterName = self.getTestFlavor(options) + '.ini'
            masterPath = os.path.join(SCRIPT_DIR, self.testRoot, masterName)

            if os.path.exists(masterPath):
                manifest = TestManifest([masterPath], strict=False)
            else:
                self._log.warning(
                    'TestManifest masterPath %s does not exist' %
                    masterPath)

        return manifest

    def getDirectories(self, options):
        """
            Make the list of directories by parsing manifests
        """
        tests = self.getActiveTests(options)
        dirlist = []
        for test in tests:
            if 'disabled' in test:
                continue

            rootdir = '/'.join(test['path'].split('/')[:-1])
            if rootdir not in dirlist:
                dirlist.append(rootdir)

        return dirlist


def main():
    # parse command line options
    parser = MochitestOptions()
    commandline.add_logging_group(parser)
    options = parser.parse_args()
    if options is None:
        # parsing error
        sys.exit(1)
    logger_options = {
        key: value for key,
        value in vars(options).iteritems() if key.startswith('log')}
    mochitest = Mochitest(logger_options)
    options = parser.verifyOptions(options, mochitest)

    options.utilityPath = mochitest.getFullPath(options.utilityPath)
    options.certPath = mochitest.getFullPath(options.certPath)
    if options.symbolsPath and len(urlparse(options.symbolsPath).scheme) < 2:
        options.symbolsPath = mochitest.getFullPath(options.symbolsPath)

    return_code = mochitest.runTests(options)
    mochitest.message_logger.finish()

    sys.exit(return_code)

if __name__ == "__main__":
    main()
