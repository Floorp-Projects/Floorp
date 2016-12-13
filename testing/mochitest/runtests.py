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
import ctypes
import glob
import json
import mozcrash
import mozdebug
import mozinfo
import mozprocess
import mozrunner
import numbers
import platform
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import traceback
import urllib2
import uuid
import zipfile
import bisection

from datetime import datetime
from manifestparser import TestManifest
from manifestparser.filters import (
    chunk_by_dir,
    chunk_by_runtime,
    chunk_by_slice,
    pathprefix,
    subsuite,
    tags,
)

try:
    from marionette_driver.addons import Addons
    from marionette_harness import Marionette
except ImportError, e:
    # Defer ImportError until attempt to use Marionette
    def reraise(*args, **kwargs):
        raise(e)
    Marionette = reraise

from leaks import ShutdownLeaks, LSANLeaks
from mochitest_options import (
    MochitestArgumentParser, build_obj, get_default_valgrind_suppression_files
)
from mozprofile import Profile, Preferences
from mozprofile.permissions import ServerLocations
from urllib import quote_plus as encodeURIComponent
from mozlog.formatters import TbplFormatter
from mozlog import commandline
from mozrunner.utils import get_stack_fixer_function, test_environment
from mozscreenshot import dump_screen
import mozleak

HAVE_PSUTIL = False
try:
    import psutil
    HAVE_PSUTIL = True
except ImportError:
    pass

here = os.path.abspath(os.path.dirname(__file__))


########################################
# Option for MOZ (former NSPR) logging #
########################################

# Set the desired log modules you want a log be produced
# by a try run for, or leave blank to disable the feature.
# This will be passed to MOZ_LOG environment variable.
# Try run will then put a download link for all log files
# on tbpl.mozilla.org.

MOZ_LOG = ""

#####################
# Test log handling #
#####################

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

        # Even if buffering is enabled, we only want to buffer messages between
        # TEST-START/TEST-END. So it is off to begin, but will be enabled after
        # a TEST-START comes in.
        self.buffering = False
        self.restore_buffering = buffering

        # Message buffering
        self.buffered_messages = []

        # Failures reporting, after the end of the tests execution
        self.errors = []

    def valid_message(self, obj):
        """True if the given object is a valid structured message
        (only does a superficial validation)"""
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

    def _fix_message_format(self, message):
        if 'message' in message:
            if isinstance(message['message'], bytes):
                message['message'] = message['message'].decode('utf-8', 'replace')
            elif not isinstance(message['message'], unicode):
                message['message'] = unicode(message['message'])

    def parse_line(self, line):
        """Takes a given line of input (structured or not) and
        returns a list of structured messages"""
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
            self._fix_message_format(message)
            messages.append(message)

        return messages

    def process_message(self, message):
        """Processes a structured message. Takes into account buffering, errors, ..."""
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
                  message['action'] not in self.BUFFERED_ACTIONS]):
            self.logger.log_raw(message)
        else:
            # Buffering the message
            self.buffered_messages.append(message)

        # If a test ended, we clean the buffer
        if message['action'] == 'test_end':
            self.buffered_messages = []
            self.restore_buffering = self.restore_buffering or self.buffering
            self.buffering = False

        if message['action'] == 'test_start':
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
            dumped_messages = self.buffered_messages[-self.BUFFERING_THRESHOLD:]
        else:
            dumped_messages = self.buffered_messages

        last_timestamp = None
        for buf in dumped_messages:
            timestamp = datetime.fromtimestamp(buf['time'] / 1000).strftime('%H:%M:%S')
            if timestamp != last_timestamp:
                self.logger.info("Buffered messages logged at {}".format(timestamp))
            last_timestamp = timestamp

            self.logger.log_raw(buf)
        self.logger.info("Buffered messages finished")
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
        self._keep_open = bool(options['keep_open'])
        self._utilityPath = options['utilityPath']
        self._xrePath = options['xrePath']
        self._profileDir = options['profilePath']
        self.webServer = options['webServer']
        self.httpPort = options['httpPort']
        self.shutdownURL = "http://%(server)s:%(port)s/server/shutdown" % {
            "server": self.webServer,
            "port": self.httpPort}
        self.testPrefix = "undefined"

        if options.get('httpdPath'):
            self._httpdPath = options['httpdPath']
        else:
            self._httpdPath = SCRIPT_DIR
        self._httpdPath = os.path.abspath(self._httpdPath)

    def start(self):
        "Run the Mochitest server, returning the process ID of the server."

        # get testing environment
        env = test_environment(xrePath=self._xrePath, log=self._log)
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
            "const _PROFILE_PATH = '%(profile)s'; const _SERVER_PORT = '%(port)s'; "
            "const _SERVER_ADDR = '%(server)s'; const _TEST_PREFIX = %(testPrefix)s; "
            "const _DISPLAY_RESULTS = %(displayResults)s;" % {
                "profile": self._profileDir.replace(
                    '\\',
                    '\\\\'),
                "port": self.httpPort,
                "server": self.webServer,
                "testPrefix": self.testPrefix,
                "displayResults": str(
                    self._keep_open).lower()},
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
            time.sleep(.05)
            i += .05
        else:
            self._log.error(
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
                    'tls1',
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

        env = test_environment(xrePath=self.xrePath, log=self.log)
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


class MochitestDesktop(object):
    """
    Mochitest class for desktop firefox.
    """
    oldcwd = os.getcwd()
    mochijar = os.path.join(SCRIPT_DIR, 'mochijar')

    # Path to the test script on the server
    TEST_PATH = "tests"
    NESTED_OOP_TEST_PATH = "nested_oop"
    CHROME_PATH = "redirect.html"
    log = None

    certdbNew = False
    sslTunnel = None
    DEFAULT_TIMEOUT = 60.0
    mediaDevices = None

    # XXX use automation.py for test name to avoid breaking legacy
    # TODO: replace this with 'runtests.py' or 'mochitest' or the like
    test_name = 'automation.py'

    def __init__(self, logger_options, quiet=False):
        self.update_mozinfo()
        self.server = None
        self.wsserver = None
        self.websocketProcessBridge = None
        self.sslTunnel = None
        self._active_tests = None
        self._locations = None

        self.marionette = None
        self.start_script = None
        self.mozLogs = None
        self.start_script_args = []
        self.urlOpts = []

        if self.log is None:
            commandline.log_formatters["tbpl"] = (
                MochitestFormatter,
                "Mochitest specific tbpl formatter")
            self.log = commandline.setup_logging("mochitest",
                                                 logger_options,
                                                 {
                                                     "tbpl": sys.stdout
                                                 })
            MochitestDesktop.log = self.log

        self.message_logger = MessageLogger(logger=self.log, buffering=quiet)
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

        self.start_script = os.path.join(here, 'start_desktop.js')

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

    def environment(self, **kwargs):
        kwargs['log'] = self.log
        return test_environment(**kwargs)

    def extraPrefs(self, extraPrefs):
        """interpolate extra preferences from option strings"""

        try:
            return dict(parseKeyValue(extraPrefs, context='--setpref='))
        except KeyValueParseError as e:
            print str(e)
            sys.exit(1)

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

        if options.flavor in ('a11y', 'browser', 'chrome', 'jetpack-addon', 'jetpack-package'):
            self.makeTestConfig(options)
        else:
            if options.autorun:
                self.urlOpts.append("autorun=1")
            if options.timeout:
                self.urlOpts.append("timeout=%d" % options.timeout)
            if options.maxTimeouts:
                self.urlOpts.append("maxTimeouts=%d" % options.maxTimeouts)
            if not options.keep_open:
                self.urlOpts.append("closeWhenDone=1")
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
            if len(options.test_paths) == 1 and options.repeat > 0 and os.path.isfile(
                os.path.join(
                    self.oldcwd,
                    os.path.dirname(__file__),
                    self.TEST_PATH,
                    options.test_paths[0])):
                self.urlOpts.append("testname=%s" % "/".join(
                    [self.TEST_PATH, options.test_paths[0]]))
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

    def normflavor(self, flavor):
        """
        In some places the string 'browser-chrome' is expected instead of
        'browser' and 'mochitest' instead of 'plain'. Normalize the flavor
        strings for those instances.
        """
        # TODO Use consistent flavor strings everywhere and remove this
        if flavor == 'browser':
            return 'browser-chrome'
        elif flavor == 'plain':
            return 'mochitest'
        return flavor

    # This check can be removed when bug 983867 is fixed.
    def isTest(self, options, filename):
        allow_js_css = False
        if options.flavor == 'browser':
            allow_js_css = True
            testPattern = re.compile(r"browser_.+\.js")
        elif options.flavor == 'jetpack-package':
            allow_js_css = True
            testPattern = re.compile(r"test-.+\.js")
        elif options.flavor == 'jetpack-addon':
            testPattern = re.compile(r".+\.xpi")
        elif options.flavor in ('a11y', 'chrome'):
            testPattern = re.compile(r"(browser|test)_.+\.(xul|html|js|xhtml)")
        else:
            testPattern = re.compile(r"test_")

        if not allow_js_css and (".js" in filename or ".css" in filename):
            return False

        pathPieces = filename.split("/")

        return (testPattern.match(pathPieces[-1]) and
                not re.search(r'\^headers\^$', filename))

    def setTestRoot(self, options):
        if options.flavor != 'plain':
            self.testRoot = options.flavor

            if options.flavor == 'browser' and options.immersiveMode:
                self.testRoot = 'metro'
        else:
            self.testRoot = self.TEST_PATH
        self.testRootAbs = os.path.join(SCRIPT_DIR, self.testRoot)

    def buildTestURL(self, options):
        testHost = "http://mochi.test:8888"
        testURL = "/".join([testHost, self.TEST_PATH])

        if len(options.test_paths) == 1:
            if options.repeat > 0 and os.path.isfile(
                os.path.join(
                    self.oldcwd,
                    os.path.dirname(__file__),
                    self.TEST_PATH,
                    options.test_paths[0])):
                testURL = "/".join([testURL, os.path.dirname(options.test_paths[0])])
            else:
                testURL = "/".join([testURL, options.test_paths[0]])

        if options.flavor in ('a11y', 'chrome'):
            testURL = "/".join([testHost, self.CHROME_PATH])
        elif options.flavor in ('browser', 'jetpack-addon', 'jetpack-package'):
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

    def startWebsocketProcessBridge(self, options):
        """Create a websocket server that can launch various processes that
        JS needs (eg; ICE server for webrtc testing)
        """

        command = [sys.executable,
                   os.path.join("websocketprocessbridge",
                                "websocketprocessbridge.py")]
        self.websocketProcessBridge = mozprocess.ProcessHandler(command,
                                                                cwd=SCRIPT_DIR)
        self.websocketProcessBridge.run()
        self.log.info("runtests.py | websocket/process bridge pid: %d"
                      % self.websocketProcessBridge.pid)

        # ensure the server is up, wait for at most ten seconds
        for i in range(1, 100):
            if self.websocketProcessBridge.proc.poll() is not None:
                self.log.error("runtests.py | websocket/process bridge failed "
                               "to launch. Are all the dependencies installed?")
                return

            try:
                sock = socket.create_connection(("127.0.0.1", 8191))
                sock.close()
                break
            except:
                time.sleep(0.1)
        else:
            self.log.error("runtests.py | Timed out while waiting for "
                           "websocket/process bridge startup.")

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

        if options.subsuite in ["media"]:
            self.startWebsocketProcessBridge(options)

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

        if self.websocketProcessBridge is not None:
            try:
                self.websocketProcessBridge.kill()
                self.log.info('Stopping websocket/process bridge')
            except Exception:
                self.log.critical('Exception stopping websocket/process bridge')

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
        if options.store_chrome_manifest:
            shutil.copyfile(manifest, options.store_chrome_manifest)
        return manifest

    def addChromeToProfile(self, options):
        "Adds MochiKit chrome tests to the profile."

        # Create (empty) chrome directory.
        chromedir = os.path.join(options.profilePath, "chrome")
        os.mkdir(chromedir)

        # Write userChrome.css.
        chrome = """
/* set default namespace to XUL */
@namespace url("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul");
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

        if not os.path.isdir(self.mochijar):
            self.log.error(
                "TEST-UNEXPECTED-FAIL | invalid setup: missing mochikit extension")
            return None

        return manifest

    def getExtensionsToInstall(self, options):
        "Return a list of extensions to install in the profile"
        extensions = []
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
        extensions.extend(options.extensionsToInstall)
        return extensions

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

    def getActiveTests(self, options, disabled=True):
        """
          This method is used to parse the manifest and return active filtered tests.
        """
        if self._active_tests:
            return self._active_tests

        tests = []
        manifest = self.getTestManifest(options)
        if manifest:
            if options.extra_mozinfo_json:
                mozinfo.update(options.extra_mozinfo_json)
            info = mozinfo.info

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

            if options.test_tags:
                filters.append(tags(options.test_tags))

            if options.test_paths:
                options.test_paths = self.normalize_paths(options.test_paths)
                filters.append(pathprefix(options.test_paths))

            # Add chunking filters if specified
            if options.totalChunks:
                if options.chunkByRuntime:
                    runtime_file = self.resolve_runtime_file(options)
                    if not os.path.exists(runtime_file):
                        self.log.warning("runtime file %s not found; defaulting to chunk-by-dir" %
                                         runtime_file)
                        options.chunkByRuntime = None
                        if options.flavor == 'browser':
                            # these values match current mozharness configs
                            options.chunkbyDir = 5
                        else:
                            options.chunkByDir = 4

                if options.chunkByDir:
                    filters.append(chunk_by_dir(options.thisChunk,
                                                options.totalChunks,
                                                options.chunkByDir))
                elif options.chunkByRuntime:
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
        if options.dump_tests:
            options.dump_tests = os.path.expanduser(options.dump_tests)
            assert os.path.exists(os.path.dirname(options.dump_tests))
            with open(options.dump_tests, 'w') as dumpFile:
                dumpFile.write(json.dumps({'active_tests': self._active_tests}))

            self.log.info("Dumping active_tests to %s file." % options.dump_tests)
            sys.exit()

        return self._active_tests

    def getTestManifest(self, options):
        if isinstance(options.manifestFile, TestManifest):
            manifest = options.manifestFile
        elif options.manifestFile and os.path.isfile(options.manifestFile):
            manifestFileAbs = os.path.abspath(options.manifestFile)
            assert manifestFileAbs.startswith(SCRIPT_DIR)
            manifest = TestManifest([options.manifestFile], strict=False)
        elif (options.manifestFile and
                os.path.isfile(os.path.join(SCRIPT_DIR, options.manifestFile))):
            manifestFileAbs = os.path.abspath(
                os.path.join(
                    SCRIPT_DIR,
                    options.manifestFile))
            assert manifestFileAbs.startswith(SCRIPT_DIR)
            manifest = TestManifest([manifestFileAbs], strict=False)
        else:
            masterName = self.normflavor(options.flavor) + '.ini'
            masterPath = os.path.join(SCRIPT_DIR, self.testRoot, masterName)

            if os.path.exists(masterPath):
                manifest = TestManifest([masterPath], strict=False)
            else:
                manifest = None
                self.log.warning(
                    'TestManifest masterPath %s does not exist' %
                    masterPath)

        return manifest

    def makeTestConfig(self, options):
        "Creates a test configuration file for customizing test execution."
        options.logFile = options.logFile.replace("\\", "\\\\")

        if "MOZ_HIDE_RESULTS_TABLE" in os.environ and os.environ[
                "MOZ_HIDE_RESULTS_TABLE"] == "1":
            options.hideResultsTable = True

        # strip certain unnecessary items to avoid serialization errors in json.dumps()
        d = dict((k, v) for k, v in options.__dict__.items() if (v is None) or
                 isinstance(v, (basestring, numbers.Number)))
        d['testRoot'] = self.testRoot
        if options.jscov_dir_prefix:
            d['jscovDirPrefix'] = options.jscov_dir_prefix
        if not options.keep_open:
            d['closeWhenDone'] = '1'
        content = json.dumps(d)

        with open(os.path.join(options.profilePath, "testConfig.js"), "w") as config:
            config.write(content)

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
        if options.flavor == 'plain':
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

        # Produce a mozlog, if setup (see MOZ_LOG global at the top of
        # this script).
        self.mozLogs = MOZ_LOG and "MOZ_UPLOAD_DIR" in os.environ
        if self.mozLogs:
            browserEnv["MOZ_LOG"] = MOZ_LOG

        if debugger and not options.slowscript:
            browserEnv["JS_DISABLE_SLOW_SCRIPT_SIGNALS"] = "1"

        # For e10s, our tests default to suppressing the "unsafe CPOW usage"
        # warnings that can plague test logs.
        if not options.enableCPOWWarnings:
            browserEnv["DISABLE_UNSAFE_CPOW_WARNINGS"] = "1"

        return browserEnv

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

    def execute_start_script(self):
        if not self.start_script or not self.marionette:
            return

        if os.path.isfile(self.start_script):
            with open(self.start_script, 'r') as fh:
                script = fh.read()
        else:
            script = self.start_script

        with self.marionette.using_context('chrome'):
            return self.marionette.execute_script(script, script_args=self.start_script_args)

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
            # android uses the new DB formats exclusively
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
        if options.flavor == 'browser' and options.timeout:
            options.extraPrefs.append(
                "testing.browserTestHarness.timeout=%d" %
                options.timeout)
        # browser-chrome tests use a fairly short default timeout of 45 seconds;
        # this is sometimes too short on asan, where we expect reduced performance.
        if mozinfo.info["asan"] and options.flavor == 'browser' and options.timeout is None:
            self.log.info("Increasing default timeout to 90 seconds on ASAN")
            options.extraPrefs.append("testing.browserTestHarness.timeout=90")

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

        # preferences
        preferences = [
            os.path.join(
                SCRIPT_DIR,
                'profile_data',
                'prefs_general.js')]

        prefs = {}
        for path in preferences:
            prefs.update(Preferences.read_prefs(path))

        prefs.update(self.extraPrefs(options.extraPrefs))

        # Bug 1262954: For windows XP + e10s disable acceleration
        if platform.system() in ("Windows", "Microsoft") and \
           '5.1' in platform.version() and options.e10s:
            prefs['layers.acceleration.disabled'] = True

        # interpolate preferences
        interpolation = {
            "server": "%s:%s" %
            (options.webServer, options.httpPort)}

        prefs = json.loads(json.dumps(prefs) % interpolation)
        for pref in prefs:
            prefs[pref] = Preferences.cast(prefs[pref])
        # TODO: make this less hacky
        # https://bugzilla.mozilla.org/show_bug.cgi?id=913152

        # proxy
        # use SSL port for legacy compatibility; see
        # - https://bugzilla.mozilla.org/show_bug.cgi?id=688667#c66
        # - https://bugzilla.mozilla.org/show_bug.cgi?id=899221
        # - https://github.com/mozilla/mozbase/commit/43f9510e3d58bfed32790c82a57edac5f928474d
        #             'ws': str(self.webSocketPort)
        proxy = {'remote': options.webServer,
                 'http': options.httpPort,
                 'https': options.sslPort,
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
            self.log.error(
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
        dump_screen(utilityPath, self.log)

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
        self.log.info("Killing process: %s" % processPID)
        if dump_screen:
            self.dumpScreen(utilityPath)

        if mozinfo.info.get('crashreporter', True) and not debuggerInfo:
            try:
                minidump_path = os.path.join(self.profile.profile,
                                             'minidumps')
                mozcrash.kill_and_get_minidump(processPID, minidump_path,
                                               utilityPath)
            except OSError:
                # https://bugzilla.mozilla.org/show_bug.cgi?id=921509
                self.log.info(
                    "Can't trigger Breakpad, process no longer exists")
            return
        self.log.info("Can't trigger Breakpad, just killing process")
        killPid(processPID, self.log)

    def extract_child_pids(self, process_log, parent_pid=None):
        """Parses the given log file for the pids of any processes launched by
        the main process and returns them as a list.
        If parent_pid is provided, and psutil is available, returns children of
        parent_pid according to psutil.
        """
        if parent_pid and HAVE_PSUTIL:
            self.log.info("Determining child pids from psutil")
            return [p.pid for p in psutil.Process(parent_pid).children()]

        rv = []
        pid_re = re.compile(r'==> process \d+ launched child process (\d+)')
        with open(process_log) as fd:
            for line in fd:
                self.log.info(line.rstrip())
                m = pid_re.search(line)
                if m:
                    rv.append(int(m.group(1)))
        return rv

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
        processList = self.extract_child_pids(processLog)
        # kill zombies
        foundZombie = False
        for processPID in processList:
            self.log.info(
                "zombiecheck | Checking for orphan process with PID: %d" %
                processPID)
            if isPidAlive(processPID):
                foundZombie = True
                self.log.error("TEST-UNEXPECTED-FAIL | zombiecheck | child process "
                               "%d still alive after shutdown" % processPID)
                self.killAndGetStack(
                    processPID,
                    utilityPath,
                    debuggerInfo,
                    dump_screen=not debuggerInfo)

        return foundZombie

    def runApp(self,
               testUrl,
               env,
               app,
               profile,
               extraArgs,
               utilityPath,
               debuggerInfo=None,
               valgrindPath=None,
               valgrindArgs=None,
               valgrindSuppFiles=None,
               symbolsPath=None,
               timeout=-1,
               detectShutdownLeaks=False,
               screenshotOnFail=False,
               bisectChunk=None,
               marionette_args=None):
        """
        Run the app, log the duration it took to execute, return the status code.
        Kills the app if it runs for longer than |maxTime| seconds, or outputs nothing
        for |timeout| seconds.
        """
        # It can't be the case that both a with-debugger and an
        # on-Valgrind run have been requested.  doTests() should have
        # already excluded this possibility.
        assert not(valgrindPath and debuggerInfo)

        # debugger information
        interactive = False
        debug_args = None
        if debuggerInfo:
            interactive = debuggerInfo.interactive
            debug_args = [debuggerInfo.path] + debuggerInfo.args

        # Set up Valgrind arguments.
        if valgrindPath:
            interactive = False
            valgrindArgs_split = ([] if valgrindArgs is None
                                  else valgrindArgs.split(","))

            valgrindSuppFiles_final = []
            if valgrindSuppFiles is not None:
                valgrindSuppFiles_final = ["--suppressions=" +
                                           path for path in valgrindSuppFiles.split(",")]

            debug_args = ([valgrindPath]
                          + mozdebug.get_default_valgrind_args()
                          + valgrindArgs_split
                          + valgrindSuppFiles_final)

        # fix default timeout
        if timeout == -1:
            timeout = self.DEFAULT_TIMEOUT

        # Note in the log if running on Valgrind
        if valgrindPath:
            self.log.info("runtests.py | Running on Valgrind.  "
                          + "Using timeout of %d seconds." % timeout)

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
            args.append('-marionette')
            # TODO: mozrunner should use -foreground at least for mac
            # https://bugzilla.mozilla.org/show_bug.cgi?id=916512
            args.append('-foreground')
            self.start_script_args.append(testUrl or 'about:blank')

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
                    processLog)
            kp_kwargs = {'kill_on_timeout': False,
                         'cwd': SCRIPT_DIR,
                         'onTimeout': [timeoutHandler]}
            kp_kwargs['processOutputLine'] = [outputHandler]

            # create mozrunner instance and start the system under test process
            self.lastTestSeen = self.test_name
            startTime = datetime.now()

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
            self.log.process_start("Main app process")

            # start marionette and kick off the tests
            marionette_args = marionette_args or {}
            port_timeout = marionette_args.pop('port_timeout')
            self.marionette = Marionette(**marionette_args)
            self.marionette.start_session(timeout=port_timeout)

            # install specialpowers and mochikit as temporary addons
            addons = Addons(self.marionette)

            if mozinfo.info.get('toolkit') != 'gonk':
                addons.install(os.path.join(here, 'extensions', 'specialpowers'), temp=True)
                addons.install(self.mochijar, temp=True)

            self.execute_start_script()

            # an open marionette session interacts badly with mochitest,
            # delete it until we figure out why.
            self.marionette.delete_session()
            del self.marionette

            # wait until app is finished
            # XXX copy functionality from
            # https://github.com/mozilla/mozbase/blob/master/mozrunner/mozrunner/runner.py#L61
            # until bug 913970 is fixed regarding mozrunner `wait` not returning status
            # see https://bugzilla.mozilla.org/show_bug.cgi?id=913970
            status = proc.wait()
            self.log.process_exit("Main app process", status)
            runner.process_handler = None

            # finalize output handler
            outputHandler.finish()

            # record post-test information
            if status:
                self.message_logger.dump_buffered()
                self.log.error(
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

    def resolve_runtime_file(self, options):
        """
        Return a path to the runtimes file for a given flavor and
        subsuite.
        """
        template = "mochitest{e10s}-{suite_slug}.runtimes.json"
        data_dir = os.path.join(SCRIPT_DIR, 'runtimes')

        # Determine the suite slug in the runtimes file name
        slug = self.normflavor(options.flavor)
        if slug == 'browser-chrome' and options.subsuite == 'devtools':
            slug = 'devtools-chrome'
        elif slug == 'mochitest':
            slug = 'plain'
            if options.subsuite:
                slug = options.subsuite

        e10s = ''
        if options.e10s:
            e10s = '-e10s'

        return os.path.join(data_dir, template.format(
            e10s=e10s, suite_slug=slug))

    def normalize_paths(self, paths):
        # Normalize test paths so they are relative to test root
        norm_paths = []
        for p in paths:
            abspath = os.path.abspath(os.path.join(self.oldcwd, p))
            if abspath.startswith(self.testRootAbs):
                norm_paths.append(os.path.relpath(abspath, self.testRootAbs))
            else:
                norm_paths.append(p)
        return norm_paths

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

    def runMochitests(self, options, testsToRun):
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
                    self.log.error("TEST-UNEXPECTED-FAIL | Bisection | Please ignore repeats "
                                   "and look for 'Bleedthrough' (if any) at the end of "
                                   "the failure list")
                    bisection_log = 1

            result = self.doTests(options, testsToRun)
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

    def runTests(self, options):
        """ Prepare, configure, run tests and cleanup """

        # a11y and chrome tests don't run with e10s enabled in CI. Need to set
        # this here since |mach mochitest| sets the flavor after argument parsing.
        if options.flavor in ('a11y', 'chrome'):
            options.e10s = False
        mozinfo.update({"e10s": options.e10s})  # for test manifest parsing.

        self.setTestRoot(options)

        # Despite our efforts to clean up servers started by this script, in practice
        # we still see infrequent cases where a process is orphaned and interferes
        # with future tests, typically because the old server is keeping the port in use.
        # Try to avoid those failures by checking for and killing orphan servers before
        # trying to start new ones.
        self.killNamedOrphans('ssltunnel')
        self.killNamedOrphans('xpcshell')

        if options.cleanupCrashes:
            mozcrash.cleanup_pending_crash_reports()

        # Until we have all green, this only runs on bc*/dt*/mochitest-chrome
        # jobs, not jetpack*, a11yr (for perf reasons), or plain

        testsToRun = self.getTestsToRun(options)
        if not options.runByDir:
            return self.runMochitests(options, testsToRun)

        # code for --run-by-dir
        dirs = self.getDirectories(options)

        result = 1  # default value, if no tests are run.
        for d in dirs:
            print "dir: %s" % d
            tests_in_dir = [t for t in testsToRun if os.path.dirname(t) == d]

            # If we are using --run-by-dir, we should not use the profile path (if) provided
            # by the user, since we need to create a new directory for each run. We would face
            # problems if we use the directory provided by the user.
            result = self.runMochitests(options, tests_in_dir)

            # Dump the logging buffer
            self.message_logger.dump_buffered()

            if result == -1:
                break

        e10s_mode = "e10s" if options.e10s else "non-e10s"

        # printing total number of tests
        if options.flavor == 'browser':
            print "TEST-INFO | checking window state"
            print "Browser Chrome Test Summary"
            print "\tPassed: %s" % self.countpass
            print "\tFailed: %s" % self.countfail
            print "\tTodo: %s" % self.counttodo
            print "\tMode: %s" % e10s_mode
            print "*** End BrowserChrome Test Results ***"
        else:
            print "0 INFO TEST-START | Shutdown"
            print "1 INFO Passed:  %s" % self.countpass
            print "2 INFO Failed:  %s" % self.countfail
            print "3 INFO Todo:    %s" % self.counttodo
            print "4 INFO Mode:    %s" % e10s_mode
            print "5 INFO SimpleTest FINISHED"

        return result

    def doTests(self, options, testsToFilter=None):
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

        # See if we were asked to run on Valgrind
        valgrindPath = None
        valgrindArgs = None
        valgrindSuppFiles = None
        if options.valgrind:
            valgrindPath = options.valgrind
        if options.valgrindArgs:
            valgrindArgs = options.valgrindArgs
        if options.valgrindSuppFiles:
            valgrindSuppFiles = options.valgrindSuppFiles

        if (valgrindArgs or valgrindSuppFiles) and not valgrindPath:
            self.log.error("Specified --valgrind-args or --valgrind-supp-files,"
                           " but not --valgrind")
            return 1

        if valgrindPath and debuggerInfo:
            self.log.error("Can't use both --debugger and --valgrind together")
            return 1

        if valgrindPath and not valgrindSuppFiles:
            valgrindSuppFiles = ",".join(get_default_valgrind_suppression_files())

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

        if self.browserEnv is None:
            return 1

        if self.mozLogs:
            self.browserEnv["MOZ_LOG_FILE"] = "{}/moz-pid=%PID-uid={}.log".format(
                self.browserEnv["MOZ_UPLOAD_DIR"], str(uuid.uuid4()))

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

            # Detect shutdown leaks for m-bc runs if
            # code coverage is not enabled.
            detectShutdownLeaks = False
            if options.jscov_dir_prefix is None:
                detectShutdownLeaks = mozinfo.info[
                    "debug"] and options.flavor == 'browser'

            self.start_script_args.append(self.normflavor(options.flavor))
            marionette_args = {
                'symbols_path': options.symbolsPath,
                'socket_timeout': options.marionette_socket_timeout,
                'port_timeout': options.marionette_port_timeout,
            }

            if options.marionette:
                host, port = options.marionette.split(':')
                marionette_args['host'] = host
                marionette_args['port'] = int(port)

            self.log.info("runtests.py | Running with e10s: {}".format(options.e10s))
            self.log.info("runtests.py | Running tests: start.\n")
            status = self.runApp(testURL,
                                 self.browserEnv,
                                 options.app,
                                 profile=self.profile,
                                 extraArgs=options.browserArgs,
                                 utilityPath=options.utilityPath,
                                 debuggerInfo=debuggerInfo,
                                 valgrindPath=valgrindPath,
                                 valgrindArgs=valgrindArgs,
                                 valgrindSuppFiles=valgrindSuppFiles,
                                 symbolsPath=options.symbolsPath,
                                 timeout=timeout,
                                 detectShutdownLeaks=detectShutdownLeaks,
                                 screenshotOnFail=options.screenshotOnFail,
                                 bisectChunk=options.bisectChunk,
                                 marionette_args=marionette_args,
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
            self.stopServers()

        ignoreMissingLeaks = options.ignoreMissingLeaks
        leakThresholds = options.leakThresholds

        # Stop leak detection if m-bc code coverage is enabled
        # by maxing out the leak threshold for all processes.
        if options.jscov_dir_prefix:
            for processType in leakThresholds:
                ignoreMissingLeaks.append(processType)
                leakThresholds[processType] = sys.maxsize

        mozleak.process_leak_log(
            self.leak_report_file,
            leak_thresholds=leakThresholds,
            ignore_missing_leaks=ignoreMissingLeaks,
            log=self.log,
            stack_fixer=get_stack_fixer_function(options.utilityPath,
                                                 options.symbolsPath),
        )

        self.log.info("runtests.py | Running tests: end.")

        if self.manifest is not None:
            self.cleanup(options)

        return status

    def handleTimeout(self, timeout, proc, utilityPath, debuggerInfo,
                      browser_pid, processLog):
        """handle process output timeout"""
        # TODO: bug 913975 : _processOutput should call self.processOutputLine
        # one more time one timeout (I think)
        error_message = ("TEST-UNEXPECTED-TIMEOUT | %s | application timed out after "
                         "%d seconds with no output") % (self.lastTestSeen, int(timeout))
        self.message_logger.dump_buffered()
        self.message_logger.buffering = False
        self.log.info(error_message)
        self.log.error("Force-terminating active process(es).")

        browser_pid = browser_pid or proc.pid
        child_pids = self.extract_child_pids(processLog, browser_pid)
        self.log.info('Found child pids: %s' % child_pids)

        if HAVE_PSUTIL:
            child_procs = [psutil.Process(pid) for pid in child_pids]
            for pid in child_pids:
                self.killAndGetStack(pid, utilityPath, debuggerInfo,
                                     dump_screen=not debuggerInfo)
            gone, alive = psutil.wait_procs(child_procs, timeout=30)
            for p in gone:
                self.log.info('psutil found pid %s dead' % p.pid)
            for p in alive:
                self.log.warning('failed to kill pid %d after 30s' %
                                 p.pid)
        else:
            self.log.error("psutil not available! Will wait 30s before "
                           "attempting to kill parent process. This should "
                           "not occur in mozilla automation. See bug 1143547.")
            for pid in child_pids:
                self.killAndGetStack(pid, utilityPath, debuggerInfo,
                                     dump_screen=not debuggerInfo)
            if child_pids:
                time.sleep(30)

        self.killAndGetStack(browser_pid, utilityPath, debuggerInfo,
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

            # With metro browser runs this script launches the metro test harness which launches
            # the browser. The metro test harness hands back the real browser process id via log
            # output which we need to pick up on and parse out. This variable tracks the real
            # browser process id if we find it.
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
            return get_stack_fixer_function, if any, to use on the output lines
            """
            return get_stack_fixer_function(self.utilityPath, self.symbolsPath)

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
            except (AttributeError, ValueError):
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
                message['message'] = self.stackFixerFunction(message['message'])
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


def run_test_harness(parser, options):
    parser.validate(options)

    logger_options = {
        key: value for key, value in vars(options).iteritems()
        if key.startswith('log') or key == 'valgrind'}

    runner = MochitestDesktop(logger_options, quiet=options.quiet)

    options.runByDir = False

    if options.flavor in ('plain', 'browser', 'chrome'):
        options.runByDir = True

    result = runner.runTests(options)

    if runner.mozLogs:
        with zipfile.ZipFile("{}/mozLogs.zip".format(runner.browserEnv["MOZ_UPLOAD_DIR"]),
                             "w", zipfile.ZIP_DEFLATED) as logzip:
            for logfile in glob.glob("{}/moz*.log*".format(runner.browserEnv["MOZ_UPLOAD_DIR"])):
                logzip.write(logfile)
                os.remove(logfile)
            logzip.close()

    # don't dump failures if running from automation as treeherder already displays them
    if build_obj:
        if runner.message_logger.errors:
            result = 1
            runner.message_logger.logger.warning("The following tests failed:")
            for error in runner.message_logger.errors:
                runner.message_logger.logger.log_raw(error)

    runner.message_logger.finish()

    return result


def cli(args=sys.argv[1:]):
    # parse command line options
    parser = MochitestArgumentParser(app='generic')
    options = parser.parse_args(args)
    if options is None:
        # parsing error
        sys.exit(1)

    return run_test_harness(parser, options)

if __name__ == "__main__":
    sys.exit(cli())
