# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the Mochitest test harness.
"""

from __future__ import absolute_import, division, print_function, with_statement
import os
import sys

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
sys.path.insert(0, SCRIPT_DIR)

from argparse import Namespace
from collections import defaultdict
from contextlib import closing
from distutils import spawn
import copy
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
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import traceback
import uuid
import zipfile
import bisection

from ctypes.util import find_library
from datetime import datetime, timedelta
from manifestparser import TestManifest
from manifestparser.util import normsep
from manifestparser.filters import (
    chunk_by_dir,
    chunk_by_runtime,
    chunk_by_slice,
    pathprefix,
    subsuite,
    tags,
    failures,
)
from mozgeckoprofiler import symbolicate_profile_json, view_gecko_profile

try:
    from marionette_driver.addons import Addons
    from marionette_driver.marionette import Marionette
except ImportError as e:  # noqa
    # Defer ImportError until attempt to use Marionette
    def reraise(*args, **kwargs):
        raise (e)  # noqa

    Marionette = reraise

from leaks import ShutdownLeaks, LSANLeaks
from mochitest_options import (
    MochitestArgumentParser,
    build_obj,
    get_default_valgrind_suppression_files,
)
from mozprofile import Profile
from mozprofile.cli import parse_preferences, parse_key_value, KeyValueParseError
from mozprofile.permissions import ServerLocations
from mozlog import commandline, get_proxy_logger
from mozrunner.utils import get_stack_fixer_function, test_environment
from mozscreenshot import dump_screen
import mozleak

HAVE_PSUTIL = False
try:
    import psutil

    HAVE_PSUTIL = True
except ImportError:
    pass

import six
from six.moves.urllib.parse import quote_plus as encodeURIComponent
from six.moves.urllib_request import urlopen

here = os.path.abspath(os.path.dirname(__file__))

NO_TESTS_FOUND = """
No tests were found for flavor '{}' and the following manifest filters:
{}

Make sure the test paths (if any) are spelt correctly and the corresponding
--flavor and --subsuite are being used. See `mach mochitest --help` for a
list of valid flavors.
""".lstrip()


########################################
# Option for MOZ (former NSPR) logging #
########################################

# Set the desired log modules you want a log be produced
# by a try run for, or leave blank to disable the feature.
# This will be passed to MOZ_LOG environment variable.
# Try run will then put a download link for a zip archive
# of all the log files on treeherder.
MOZ_LOG = ""

#####################
# Test log handling #
#####################

# output processing


class MessageLogger(object):

    """File-like object for logging messages (structured logs)"""

    BUFFERING_THRESHOLD = 100
    # This is a delimiter used by the JS side to avoid logs interleaving
    DELIMITER = u"\ue175\uee31\u2c32\uacbf"
    BUFFERED_ACTIONS = set(["test_status", "log"])
    VALID_ACTIONS = set(
        [
            "suite_start",
            "suite_end",
            "test_start",
            "test_end",
            "test_status",
            "log",
            "assertion_count",
            "buffering_on",
            "buffering_off",
        ]
    )
    # Regexes that will be replaced with an empty string if found in a test
    # name. We do this to normalize test names which may contain URLs and test
    # package prefixes.
    TEST_PATH_PREFIXES = [
        r"^/tests/",
        r"^\w+://[\w\.]+(:\d+)?(/\w+)?/(tests?|a11y|chrome|browser)/",
    ]

    def __init__(self, logger, buffering=True, structured=True):
        self.logger = logger
        self.structured = structured
        self.gecko_id = "GECKO"
        self.is_test_running = False

        # Even if buffering is enabled, we only want to buffer messages between
        # TEST-START/TEST-END. So it is off to begin, but will be enabled after
        # a TEST-START comes in.
        self._buffering = False
        self.restore_buffering = buffering

        # Guard to ensure we never buffer if this value was initially `False`
        self._buffering_initially_enabled = buffering

        # Message buffering
        self.buffered_messages = []

    def validate(self, obj):
        """Tests whether the given object is a valid structured message
        (only does a superficial validation)"""
        if not (
            isinstance(obj, dict)
            and "action" in obj
            and obj["action"] in MessageLogger.VALID_ACTIONS
        ):
            raise ValueError

    def _fix_subtest_name(self, message):
        """Make sure subtest name is a string"""
        if "subtest" in message and not isinstance(
            message["subtest"], six.string_types
        ):
            message["subtest"] = str(message["subtest"])

    def _fix_test_name(self, message):
        """Normalize a logged test path to match the relative path from the sourcedir."""
        if message.get("test") is not None:
            test = message["test"]
            for pattern in MessageLogger.TEST_PATH_PREFIXES:
                test = re.sub(pattern, "", test)
                if test != message["test"]:
                    message["test"] = test
                    break

    def _fix_message_format(self, message):
        if "message" in message:
            if isinstance(message["message"], bytes):
                message["message"] = message["message"].decode("utf-8", "replace")
            elif not isinstance(message["message"], six.text_type):
                message["message"] = six.text_type(message["message"])

    def parse_line(self, line):
        """Takes a given line of input (structured or not) and
        returns a list of structured messages"""
        if isinstance(line, six.binary_type):
            # if line is a sequence of bytes, let's decode it
            line = line.rstrip().decode("UTF-8", "replace")
        else:
            # line is in unicode - so let's use it as it is
            line = line.rstrip()

        messages = []
        for fragment in line.split(MessageLogger.DELIMITER):
            if not fragment:
                continue
            try:
                message = json.loads(fragment)
                self.validate(message)
            except ValueError:
                if self.structured:
                    message = dict(
                        action="process_output",
                        process=self.gecko_id,
                        data=fragment,
                    )
                else:
                    message = dict(
                        action="log",
                        level="info",
                        message=fragment,
                    )

            self._fix_subtest_name(message)
            self._fix_test_name(message)
            self._fix_message_format(message)
            messages.append(message)

        return messages

    @property
    def buffering(self):
        if not self._buffering_initially_enabled:
            return False
        return self._buffering

    @buffering.setter
    def buffering(self, val):
        self._buffering = val

    def process_message(self, message):
        """Processes a structured message. Takes into account buffering, errors, ..."""
        # Activation/deactivating message buffering from the JS side
        if message["action"] == "buffering_on":
            if self.is_test_running:
                self.buffering = True
            return
        if message["action"] == "buffering_off":
            self.buffering = False
            return

        # Error detection also supports "raw" errors (in log messages) because some tests
        # manually dump 'TEST-UNEXPECTED-FAIL'.
        if "expected" in message or (
            message["action"] == "log"
            and message.get("message", "").startswith("TEST-UNEXPECTED")
        ):
            self.restore_buffering = self.restore_buffering or self.buffering
            self.buffering = False
            if self.buffered_messages:
                snipped = len(self.buffered_messages) - self.BUFFERING_THRESHOLD
                if snipped > 0:
                    self.logger.info(
                        "<snipped {0} output lines - "
                        "if you need more context, please use "
                        "SimpleTest.requestCompleteLog() in your test>".format(snipped)
                    )
                # Dumping previously buffered messages
                self.dump_buffered(limit=True)

            # Logging the error message
            self.logger.log_raw(message)
        # Determine if message should be buffered
        elif (
            self.buffering
            and self.structured
            and message["action"] in self.BUFFERED_ACTIONS
        ):
            self.buffered_messages.append(message)
        # Otherwise log the message directly
        else:
            self.logger.log_raw(message)

        # If a test ended, we clean the buffer
        if message["action"] == "test_end":
            self.is_test_running = False
            self.buffered_messages = []
            self.restore_buffering = self.restore_buffering or self.buffering
            self.buffering = False

        if message["action"] == "test_start":
            self.is_test_running = True
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
            dumped_messages = self.buffered_messages[-self.BUFFERING_THRESHOLD :]
        else:
            dumped_messages = self.buffered_messages

        last_timestamp = None
        for buf in dumped_messages:
            # pylint --py3k W1619
            timestamp = datetime.fromtimestamp(buf["time"] / 1000).strftime("%H:%M:%S")
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
    log = get_proxy_logger("mochitest")

    def on_output(line):
        log.process_output(
            process=process.pid,
            data=line.decode("utf8", "replace"),
            command=process.commandline,
        )

    process = mozprocess.ProcessHandlerMixin(
        *args, processOutputLine=on_output, **kwargs
    )
    process.run()
    return process.wait()


def killPid(pid, log):
    # see also https://bugzilla.mozilla.org/show_bug.cgi?id=911249#c58

    if HAVE_PSUTIL:
        # Kill a process tree (including grandchildren) with signal.SIGTERM
        if pid == os.getpid():
            raise RuntimeError("Error: trying to kill ourselves, not another process")
        try:
            parent = psutil.Process(pid)
            children = parent.children(recursive=True)
            children.append(parent)
            for p in children:
                p.send_signal(signal.SIGTERM)
            gone, alive = psutil.wait_procs(children, timeout=30)
            for p in gone:
                log.info("psutil found pid %s dead" % p.pid)
            for p in alive:
                log.info("failed to kill pid %d after 30s" % p.pid)
        except Exception as e:
            log.info("Error: Failed to kill process %d: %s" % (pid, str(e)))
    else:
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
            PROCESS_QUERY_LIMITED_INFORMATION, 0, pid
        )
        if not pHandle:
            return False

        try:
            pExitCode = ctypes.wintypes.DWORD()
            ctypes.windll.kernel32.GetExitCodeProcess(pHandle, ctypes.byref(pExitCode))

            if pExitCode.value != STILL_ACTIVE:
                return False

            # We have a live process handle.  But Windows aggressively
            # re-uses pids, so let's attempt to verify that this is
            # actually Firefox.
            namesize = 1024
            pName = ctypes.create_string_buffer(namesize)
            namelen = ctypes.windll.kernel32.GetProcessImageFileNameA(
                pHandle, pName, namesize
            )
            if namelen == 0:
                # Still an active process, so conservatively assume it's Firefox.
                return True

            return pName.value.endswith(("firefox.exe", "plugin-container.exe"))
        finally:
            ctypes.windll.kernel32.CloseHandle(pHandle)


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
            if err.errno in (errno.ESRCH, errno.ECHILD, errno.EPERM):
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
        self._keep_open = bool(options["keep_open"])
        self._utilityPath = options["utilityPath"]
        self._xrePath = options["xrePath"]
        self._profileDir = options["profilePath"]
        self.webServer = options["webServer"]
        self.httpPort = options["httpPort"]
        if options.get("remoteWebServer") == "10.0.2.2":
            # probably running an Android emulator and 10.0.2.2 will
            # not be visible from host
            shutdownServer = "127.0.0.1"
        else:
            shutdownServer = self.webServer
        self.shutdownURL = "http://%(server)s:%(port)s/server/shutdown" % {
            "server": shutdownServer,
            "port": self.httpPort,
        }
        self.testPrefix = "undefined"

        if options.get("httpdPath"):
            self._httpdPath = options["httpdPath"]
        else:
            self._httpdPath = SCRIPT_DIR
        self._httpdPath = os.path.abspath(self._httpdPath)

    def start(self):
        "Run the Mochitest server, returning the process ID of the server."

        # get testing environment
        env = test_environment(xrePath=self._xrePath, log=self._log)
        env["XPCOM_DEBUG_BREAK"] = "warn"
        if "LD_LIBRARY_PATH" not in env or env["LD_LIBRARY_PATH"] is None:
            env["LD_LIBRARY_PATH"] = self._xrePath
        else:
            env["LD_LIBRARY_PATH"] = ":".join([self._xrePath, env["LD_LIBRARY_PATH"]])

        # When running with an ASan build, our xpcshell server will also be ASan-enabled,
        # thus consuming too much resources when running together with the browser on
        # the test machines. Try to limit the amount of resources by disabling certain
        # features.
        env["ASAN_OPTIONS"] = "quarantine_size=1:redzone=32:malloc_context_size=5"

        # Likewise, when running with a TSan build, our xpcshell server will
        # also be TSan-enabled. Except that in this case, we don't really
        # care about races in xpcshell. So disable TSan for the server.
        env["TSAN_OPTIONS"] = "report_bugs=0"

        # Don't use socket process for the xpcshell server.
        env["MOZ_DISABLE_SOCKET_PROCESS"] = "1"

        if mozinfo.isWin:
            env["PATH"] = env["PATH"] + ";" + str(self._xrePath)

        args = [
            "-g",
            self._xrePath,
            "-f",
            os.path.join(self._httpdPath, "httpd.js"),
            "-e",
            "const _PROFILE_PATH = '%(profile)s'; const _SERVER_PORT = '%(port)s'; "
            "const _SERVER_ADDR = '%(server)s'; const _TEST_PREFIX = %(testPrefix)s; "
            "const _DISPLAY_RESULTS = %(displayResults)s;"
            % {
                "profile": self._profileDir.replace("\\", "\\\\"),
                "port": self.httpPort,
                "server": self.webServer,
                "testPrefix": self.testPrefix,
                "displayResults": str(self._keep_open).lower(),
            },
            "-f",
            os.path.join(SCRIPT_DIR, "server.js"),
        ]

        xpcshell = os.path.join(
            self._utilityPath, "xpcshell" + mozinfo.info["bin_suffix"]
        )
        command = [xpcshell] + args
        self._process = mozprocess.ProcessHandler(command, cwd=SCRIPT_DIR, env=env)
        self._process.run()
        self._log.info("%s : launching %s" % (self.__class__.__name__, command))
        pid = self._process.pid
        self._log.info("runtests.py | Server pid: %d" % pid)

    def ensureReady(self, timeout):
        assert timeout >= 0

        aliveFile = os.path.join(self._profileDir, "server_alive.txt")
        i = 0
        while i < timeout:
            if os.path.exists(aliveFile):
                break
            time.sleep(0.05)
            i += 0.05
        else:
            self._log.error(
                "TEST-UNEXPECTED-FAIL | runtests.py | Timed out while waiting for server startup."
            )
            self.stop()
            sys.exit(1)

    def stop(self):
        try:
            with closing(urlopen(self.shutdownURL)) as c:
                self._log.info(six.ensure_text(c.read()))
        except Exception:
            self._log.info("Failed to stop web server on %s" % self.shutdownURL)
            traceback.print_exc()
        finally:
            if self._process is not None:
                # Kill the server immediately to avoid logging intermittent
                # shutdown crashes, sometimes observed on Windows 10.
                self._process.kill()
                self._log.info("Web server killed.")


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
        scriptPath = "pywebsocket_wrapper.py"
        script = os.path.join(self._scriptdir, scriptPath)

        cmd = [sys.executable, script]
        if self.debuggerInfo and self.debuggerInfo.interactive:
            cmd += ["--interactive"]
        cmd += [
            "-H",
            "127.0.0.1",
            "-p",
            str(self.port),
            "-w",
            self._scriptdir,
            "-l",
            os.path.join(self._scriptdir, "websock.log"),
            "--log-level=debug",
            "--allow-handlers-outside-root-dir",
        ]
        env = dict(os.environ)
        env["PYTHONPATH"] = os.pathsep.join(sys.path)
        # Start the process. Ignore stderr so that exceptions from the server
        # are not treated as failures when parsing the test log.
        self._process = mozprocess.ProcessHandler(
            cmd,
            cwd=SCRIPT_DIR,
            env=env,
            processStderrLine=lambda _: None,
        )
        self._process.run()
        pid = self._process.pid
        self._log.info("runtests.py | Websocket server pid: %d" % pid)

    def stop(self):
        if self._process is not None:
            self._process.kill()


class SSLTunnel:
    def __init__(self, options, logger):
        self.log = logger
        self.process = None
        self.utilityPath = options.utilityPath
        self.xrePath = options.xrePath
        self.certPath = options.certPath
        self.sslPort = options.sslPort
        self.httpPort = options.httpPort
        self.webServer = options.webServer
        self.webSocketPort = options.webSocketPort

        self.customCertRE = re.compile("^cert=(?P<nickname>[0-9a-zA-Z_ ]+)")
        self.clientAuthRE = re.compile("^clientauth=(?P<clientauth>[a-z]+)")
        self.redirRE = re.compile("^redir=(?P<redirhost>[0-9a-zA-Z_ .]+)")

    def writeLocation(self, config, loc):
        for option in loc.options:
            match = self.customCertRE.match(option)
            if match:
                customcert = match.group("nickname")
                config.write(
                    "listen:%s:%s:%s:%s\n"
                    % (loc.host, loc.port, self.sslPort, customcert)
                )

            match = self.clientAuthRE.match(option)
            if match:
                clientauth = match.group("clientauth")
                config.write(
                    "clientauth:%s:%s:%s:%s\n"
                    % (loc.host, loc.port, self.sslPort, clientauth)
                )

            match = self.redirRE.match(option)
            if match:
                redirhost = match.group("redirhost")
                config.write(
                    "redirhost:%s:%s:%s:%s\n"
                    % (loc.host, loc.port, self.sslPort, redirhost)
                )

            if option in (
                "tls1",
                "tls1_1",
                "tls1_2",
                "tls1_3",
                "ssl3",
                "rc4",
                "failHandshake",
            ):
                config.write(
                    "%s:%s:%s:%s\n" % (option, loc.host, loc.port, self.sslPort)
                )

    def buildConfig(self, locations, public=None):
        """Create the ssltunnel configuration file"""
        configFd, self.configFile = tempfile.mkstemp(prefix="ssltunnel", suffix=".cfg")
        with os.fdopen(configFd, "w") as config:
            config.write("httpproxy:1\n")
            config.write("certdbdir:%s\n" % self.certPath)
            config.write("forward:127.0.0.1:%s\n" % self.httpPort)
            config.write(
                "websocketserver:%s:%s\n" % (self.webServer, self.webSocketPort)
            )
            # Use "*" to tell ssltunnel to listen on the public ip
            # address instead of the loopback address 127.0.0.1. This
            # may have the side-effect of causing firewall warnings on
            # macOS and Windows. Use "127.0.0.1" to listen on the
            # loopback address.  Remote tests using physical or
            # emulated Android devices must use the public ip address
            # in order for the sslproxy to work but Desktop tests
            # which run on the same host as ssltunnel may use the
            # loopback address.
            listen_address = "*" if public else "127.0.0.1"
            config.write("listen:%s:%s:pgoserver\n" % (listen_address, self.sslPort))

            for loc in locations:
                if loc.scheme == "https" and "nocert" not in loc.options:
                    self.writeLocation(config, loc)

    def start(self):
        """ Starts the SSL Tunnel """

        # start ssltunnel to provide https:// URLs capability
        ssltunnel = os.path.join(self.utilityPath, "ssltunnel")
        if os.name == "nt":
            ssltunnel += ".exe"
        if not os.path.exists(ssltunnel):
            self.log.error(
                "INFO | runtests.py | expected to find ssltunnel at %s" % ssltunnel
            )
            exit(1)

        env = test_environment(xrePath=self.xrePath, log=self.log)
        env["LD_LIBRARY_PATH"] = self.xrePath
        self.process = mozprocess.ProcessHandler([ssltunnel, self.configFile], env=env)
        self.process.run()
        self.log.info("runtests.py | SSL tunnel pid: %d" % self.process.pid)

    def stop(self):
        """ Stops the SSL Tunnel and cleans up """
        if self.process is not None:
            self.process.kill()
        if os.path.exists(self.configFile):
            os.remove(self.configFile)


def checkAndConfigureV4l2loopback(device):
    """
    Determine if a given device path is a v4l2loopback device, and if so
    toggle a few settings on it via fcntl. Very linux-specific.

    Returns (status, device name) where status is a boolean.
    """
    if not mozinfo.isLinux:
        return False, ""

    libc = ctypes.cdll.LoadLibrary(find_library("c"))
    O_RDWR = 2
    # These are from linux/videodev2.h

    class v4l2_capability(ctypes.Structure):
        _fields_ = [
            ("driver", ctypes.c_char * 16),
            ("card", ctypes.c_char * 32),
            ("bus_info", ctypes.c_char * 32),
            ("version", ctypes.c_uint32),
            ("capabilities", ctypes.c_uint32),
            ("device_caps", ctypes.c_uint32),
            ("reserved", ctypes.c_uint32 * 3),
        ]

    VIDIOC_QUERYCAP = 0x80685600

    fd = libc.open(six.ensure_binary(device), O_RDWR)
    if fd < 0:
        return False, ""

    vcap = v4l2_capability()
    if libc.ioctl(fd, VIDIOC_QUERYCAP, ctypes.byref(vcap)) != 0:
        return False, ""

    if six.ensure_text(vcap.driver) != "v4l2 loopback":
        return False, ""

    class v4l2_control(ctypes.Structure):
        _fields_ = [("id", ctypes.c_uint32), ("value", ctypes.c_int32)]

    # These are private v4l2 control IDs, see:
    # https://github.com/umlaeute/v4l2loopback/blob/fd822cf0faaccdf5f548cddd9a5a3dcebb6d584d/v4l2loopback.c#L131
    KEEP_FORMAT = 0x8000000
    SUSTAIN_FRAMERATE = 0x8000001
    VIDIOC_S_CTRL = 0xC008561C

    control = v4l2_control()
    control.id = KEEP_FORMAT
    control.value = 1
    libc.ioctl(fd, VIDIOC_S_CTRL, ctypes.byref(control))

    control.id = SUSTAIN_FRAMERATE
    control.value = 1
    libc.ioctl(fd, VIDIOC_S_CTRL, ctypes.byref(control))
    libc.close(fd)

    return True, six.ensure_text(vcap.card)


def findTestMediaDevices(log):
    """
    Find the test media devices configured on this system, and return a dict
    containing information about them. The dict will have keys for 'audio'
    and 'video', each containing the name of the media device to use.

    If audio and video devices could not be found, return None.

    This method is only currently implemented for Linux.
    """
    if not mozinfo.isLinux:
        return None

    info = {}
    # Look for a v4l2loopback device.
    name = None
    device = None
    for dev in sorted(glob.glob("/dev/video*")):
        result, name_ = checkAndConfigureV4l2loopback(dev)
        if result:
            name = name_
            device = dev
            break

    if not (name and device):
        log.error("Couldn't find a v4l2loopback video device")
        return None

    # Feed it a frame of output so it has something to display
    gst01 = spawn.find_executable("gst-launch-0.1")
    gst010 = spawn.find_executable("gst-launch-0.10")
    gst10 = spawn.find_executable("gst-launch-1.0")
    if gst01:
        gst = gst01
    if gst010:
        gst = gst010
    else:
        gst = gst10
    subprocess.check_call(
        [
            gst,
            "--no-fault",
            "videotestsrc",
            "pattern=green",
            "num-buffers=1",
            "!",
            "v4l2sink",
            "device=%s" % device,
        ]
    )
    info["video"] = name

    # check if PulseAudio module-null-sink is loaded
    pactl = spawn.find_executable("pactl")

    if not pactl:
        log.error("Could not find pactl on system")
        return None

    try:
        o = subprocess.check_output([pactl, "list", "short", "modules"])
    except subprocess.CalledProcessError:
        log.error("Could not list currently loaded modules")
        return None

    null_sink = [x for x in o.splitlines() if b"module-null-sink" in x]

    if not null_sink:
        try:
            subprocess.check_call([pactl, "load-module", "module-null-sink"])
        except subprocess.CalledProcessError:
            log.error("Could not load module-null-sink")
            return None

    # Hardcode the name since it's always the same.
    info["audio"] = "Monitor of Null Output"
    return info


def create_zip(path):
    """
    Takes a `path` on disk and creates a zipfile with its contents. Returns a
    path to the location of the temporary zip file.
    """
    with tempfile.NamedTemporaryFile() as f:
        # `shutil.make_archive` writes to "{f.name}.zip", so we're really just
        # using `NamedTemporaryFile` as a way to get a random path.
        return shutil.make_archive(f.name, "zip", path)


def update_mozinfo():
    """walk up directories to find mozinfo.json update the info"""
    # TODO: This should go in a more generic place, e.g. mozinfo

    path = SCRIPT_DIR
    dirs = set()
    while path != os.path.expanduser("~"):
        if path in dirs:
            break
        dirs.add(path)
        path = os.path.split(path)[0]

    mozinfo.find_and_update_from_json(*dirs)


class MochitestDesktop(object):
    """
    Mochitest class for desktop firefox.
    """

    oldcwd = os.getcwd()

    # Path to the test script on the server
    TEST_PATH = "tests"
    CHROME_PATH = "redirect.html"

    certdbNew = False
    sslTunnel = None
    DEFAULT_TIMEOUT = 60.0
    mediaDevices = None

    patternFiles = {}

    # XXX use automation.py for test name to avoid breaking legacy
    # TODO: replace this with 'runtests.py' or 'mochitest' or the like
    test_name = "automation.py"

    def __init__(self, flavor, logger_options, staged_addons=None, quiet=False):
        update_mozinfo()
        self.flavor = flavor
        self.staged_addons = staged_addons
        self.server = None
        self.wsserver = None
        self.websocketProcessBridge = None
        self.sslTunnel = None
        self.manifest = None
        self.tests_by_manifest = defaultdict(list)
        self.prefs_by_manifest = defaultdict(set)
        self.env_vars_by_manifest = defaultdict(set)
        self._active_tests = None
        self.currentTests = None
        self._locations = None
        self.browserEnv = None

        self.marionette = None
        self.start_script = None
        self.mozLogs = None
        self.start_script_kwargs = {}
        self.extraPrefs = {}
        self.extraEnv = {}

        if logger_options.get("log"):
            self.log = logger_options["log"]
        else:
            self.log = commandline.setup_logging(
                "mochitest", logger_options, {"tbpl": sys.stdout}
            )

        self.message_logger = MessageLogger(
            logger=self.log, buffering=quiet, structured=True
        )

        # Max time in seconds to wait for server startup before tests will fail -- if
        # this seems big, it's mostly for debug machines where cold startup
        # (particularly after a build) takes forever.
        self.SERVER_STARTUP_TIMEOUT = 180 if mozinfo.info.get("debug") else 90

        # metro browser sub process id
        self.browserProcessId = None

        self.haveDumpedScreen = False
        # Create variables to count the number of passes, fails, todos.
        self.countpass = 0
        self.countfail = 0
        self.counttodo = 0

        self.expectedError = {}
        self.result = {}

        self.start_script = os.path.join(here, "start_desktop.js")

        # Used to temporarily serve a performance profile
        self.profiler_tempdir = None

    def environment(self, **kwargs):
        kwargs["log"] = self.log
        return test_environment(**kwargs)

    def getFullPath(self, path):
        " Get an absolute path relative to self.oldcwd."
        return os.path.normpath(os.path.join(self.oldcwd, os.path.expanduser(path)))

    def getLogFilePath(self, logFile):
        """return the log file path relative to the device we are testing on, in most cases
        it will be the full path on the local system
        """
        return self.getFullPath(logFile)

    @property
    def locations(self):
        if self._locations is not None:
            return self._locations
        locations_file = os.path.join(SCRIPT_DIR, "server-locations.txt")
        self._locations = ServerLocations(locations_file)
        return self._locations

    def buildURLOptions(self, options, env):
        """Add test control options from the command line to the url

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
        self.urlOpts = []

        if not hasattr(options, "logFile"):
            options.logFile = ""
        if not hasattr(options, "fileLevel"):
            options.fileLevel = "INFO"

        # allow relative paths for logFile
        if options.logFile:
            options.logFile = self.getLogFilePath(options.logFile)

        if options.flavor in ("a11y", "browser", "chrome"):
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
                self.urlOpts.append("logFile=" + encodeURIComponent(options.logFile))
                self.urlOpts.append(
                    "fileLevel=" + encodeURIComponent(options.fileLevel)
                )
            if options.consoleLevel:
                self.urlOpts.append(
                    "consoleLevel=" + encodeURIComponent(options.consoleLevel)
                )
            if options.startAt:
                self.urlOpts.append("startAt=%s" % options.startAt)
            if options.endAt:
                self.urlOpts.append("endAt=%s" % options.endAt)
            if options.shuffle:
                self.urlOpts.append("shuffle=1")
            if "MOZ_HIDE_RESULTS_TABLE" in env and env["MOZ_HIDE_RESULTS_TABLE"] == "1":
                self.urlOpts.append("hideResultsTable=1")
            if options.runUntilFailure:
                self.urlOpts.append("runUntilFailure=1")
            if options.repeat:
                self.urlOpts.append("repeat=%d" % options.repeat)
            if len(options.test_paths) == 1 and os.path.isfile(
                os.path.join(
                    self.oldcwd,
                    os.path.dirname(__file__),
                    self.TEST_PATH,
                    options.test_paths[0],
                )
            ):
                self.urlOpts.append(
                    "testname=%s" % "/".join([self.TEST_PATH, options.test_paths[0]])
                )
            if options.manifestFile:
                self.urlOpts.append("manifestFile=%s" % options.manifestFile)
            if options.failureFile:
                self.urlOpts.append(
                    "failureFile=%s" % self.getFullPath(options.failureFile)
                )
            if options.runSlower:
                self.urlOpts.append("runSlower=true")
            if options.debugOnFailure:
                self.urlOpts.append("debugOnFailure=true")
            if options.dumpOutputDirectory:
                self.urlOpts.append(
                    "dumpOutputDirectory=%s"
                    % encodeURIComponent(options.dumpOutputDirectory)
                )
            if options.dumpAboutMemoryAfterTest:
                self.urlOpts.append("dumpAboutMemoryAfterTest=true")
            if options.dumpDMDAfterTest:
                self.urlOpts.append("dumpDMDAfterTest=true")
            if options.debugger:
                self.urlOpts.append("interactiveDebugger=true")
            if options.jscov_dir_prefix:
                self.urlOpts.append("jscovDirPrefix=%s" % options.jscov_dir_prefix)
            if options.cleanupCrashes:
                self.urlOpts.append("cleanupCrashes=true")
            if "MOZ_XORIGIN_MOCHITEST" in env and env["MOZ_XORIGIN_MOCHITEST"] == "1":
                options.xOriginTests = True
            if options.xOriginTests:
                self.urlOpts.append("xOriginTests=true")

    def normflavor(self, flavor):
        """
        In some places the string 'browser-chrome' is expected instead of
        'browser' and 'mochitest' instead of 'plain'. Normalize the flavor
        strings for those instances.
        """
        # TODO Use consistent flavor strings everywhere and remove this
        if flavor == "browser":
            return "browser-chrome"
        elif flavor == "plain":
            return "mochitest"
        return flavor

    # This check can be removed when bug 983867 is fixed.
    def isTest(self, options, filename):
        allow_js_css = False
        if options.flavor == "browser":
            allow_js_css = True
            testPattern = re.compile(r"browser_.+\.js")
        elif options.flavor in ("a11y", "chrome"):
            testPattern = re.compile(r"(browser|test)_.+\.(xul|html|js|xhtml)")
        else:
            testPattern = re.compile(r"test_")

        if not allow_js_css and (".js" in filename or ".css" in filename):
            return False

        pathPieces = filename.split("/")

        return testPattern.match(pathPieces[-1]) and not re.search(
            r"\^headers\^$", filename
        )

    def setTestRoot(self, options):
        if options.flavor != "plain":
            self.testRoot = options.flavor
        else:
            self.testRoot = self.TEST_PATH
        self.testRootAbs = os.path.join(SCRIPT_DIR, self.testRoot)

    def buildTestURL(self, options, scheme="http"):
        if scheme == "https":
            testHost = "https://example.com:443"
        elif options.xOriginTests:
            testHost = "http://mochi.xorigin-test:8888"
        else:
            testHost = "http://mochi.test:8888"
        testURL = "/".join([testHost, self.TEST_PATH])

        if len(options.test_paths) == 1:
            if os.path.isfile(
                os.path.join(
                    self.oldcwd,
                    os.path.dirname(__file__),
                    self.TEST_PATH,
                    options.test_paths[0],
                )
            ):
                testURL = "/".join([testURL, os.path.dirname(options.test_paths[0])])
            else:
                testURL = "/".join([testURL, options.test_paths[0]])

        if options.flavor in ("a11y", "chrome"):
            testURL = "/".join([testHost, self.CHROME_PATH])
        elif options.flavor == "browser":
            testURL = "about:blank"
        return testURL

    def getTestsByScheme(self, options, testsToFilter=None, disabled=True):
        """Build the url path to the specific test harness and test file or directory
        Build a manifest of tests to run and write out a json file for the harness to read
        testsToFilter option is used to filter/keep the tests provided in the list

        disabled -- This allows to add all disabled tests on the build side
                    and then on the run side to only run the enabled ones
        """

        tests = self.getActiveTests(options, disabled)
        paths = []
        for test in tests:
            if testsToFilter and (test["path"] not in testsToFilter):
                continue
            paths.append(test)

        # Generate test by schemes
        for (scheme, grouped_tests) in self.groupTestsByScheme(paths).items():
            # Bug 883865 - add this functionality into manifestparser
            with open(
                os.path.join(SCRIPT_DIR, options.testRunManifestFile), "w"
            ) as manifestFile:
                manifestFile.write(json.dumps({"tests": grouped_tests}))
            options.manifestFile = options.testRunManifestFile
            yield (scheme, grouped_tests)

    def startWebSocketServer(self, options, debuggerInfo):
        """ Launch the websocket server """
        self.wsserver = WebSocketServer(options, SCRIPT_DIR, self.log, debuggerInfo)
        self.wsserver.start()

    def startWebServer(self, options):
        """Create the webserver and start it up"""

        self.server = MochitestServer(options, self.log)
        self.server.start()

        if options.pidFile != "":
            with open(options.pidFile + ".xpcshell.pid", "w") as f:
                f.write("%s" % self.server._process.pid)

    def startWebsocketProcessBridge(self, options):
        """Create a websocket server that can launch various processes that
        JS needs (eg; ICE server for webrtc testing)
        """

        command = [
            sys.executable,
            os.path.join("websocketprocessbridge", "websocketprocessbridge.py"),
            "--port",
            options.websocket_process_bridge_port,
        ]
        self.websocketProcessBridge = mozprocess.ProcessHandler(command, cwd=SCRIPT_DIR)
        self.websocketProcessBridge.run()
        self.log.info(
            "runtests.py | websocket/process bridge pid: %d"
            % self.websocketProcessBridge.pid
        )

        # ensure the server is up, wait for at most ten seconds
        for i in range(1, 100):
            if self.websocketProcessBridge.proc.poll() is not None:
                self.log.error(
                    "runtests.py | websocket/process bridge failed "
                    "to launch. Are all the dependencies installed?"
                )
                return

            try:
                sock = socket.create_connection(("127.0.0.1", 8191))
                sock.close()
                break
            except Exception:
                time.sleep(0.1)
        else:
            self.log.error(
                "runtests.py | Timed out while waiting for "
                "websocket/process bridge startup."
            )

    def needsWebsocketProcessBridge(self, options):
        """
        Returns a bool indicating if the current test configuration needs
        to start the websocket process bridge or not. The boils down to if
        WebRTC tests that need the bridge are present.
        """
        tests = self.getActiveTests(options)
        is_webrtc_tag_present = False
        for test in tests:
            if "webrtc" in test.get("tags", ""):
                is_webrtc_tag_present = True
                break
        return is_webrtc_tag_present and options.subsuite in ["media"]

    def startServers(self, options, debuggerInfo, public=None):
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

        # Only webrtc mochitests in the media suite need the websocketprocessbridge.
        if self.needsWebsocketProcessBridge(options):
            self.startWebsocketProcessBridge(options)

        # start SSL pipe
        self.sslTunnel = SSLTunnel(options, logger=self.log)
        self.sslTunnel.buildConfig(self.locations, public=public)
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
                self.log.info("Stopping web server")
                self.server.stop()
            except Exception:
                self.log.critical("Exception when stopping web server")

        if self.wsserver is not None:
            try:
                self.log.info("Stopping web socket server")
                self.wsserver.stop()
            except Exception:
                self.log.critical("Exception when stopping web socket server")

        if self.sslTunnel is not None:
            try:
                self.log.info("Stopping ssltunnel")
                self.sslTunnel.stop()
            except Exception:
                self.log.critical("Exception stopping ssltunnel")

        if self.websocketProcessBridge is not None:
            try:
                self.websocketProcessBridge.kill()
                self.websocketProcessBridge.wait()
                self.log.info("Stopping websocket/process bridge")
            except Exception:
                self.log.critical("Exception stopping websocket/process bridge")

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
                self.log.warning("runtests.py | Failed to copy %s to profile" % abspath)

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
                "content mochitests %s contentaccessible=yes\n" % chrometestDir
            )
            manifestFile.write(
                "content mochitests-any %s contentaccessible=yes remoteenabled=yes\n"
                % chrometestDir
            )
            manifestFile.write(
                "content mochitests-content %s contentaccessible=yes remoterequired=yes\n"
                % chrometestDir
            )

            if options.testingModulesDir is not None:
                manifestFile.write(
                    "resource testing-common file:///%s\n" % options.testingModulesDir
                )
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
        with open(
            os.path.join(options.profilePath, "userChrome.css"), "a"
        ) as chromeFile:
            chromeFile.write(chrome)

        manifest = self.writeChromeManifest(options)

        return manifest

    def getExtensionsToInstall(self, options):
        "Return a list of extensions to install in the profile"
        extensions = []
        appDir = (
            options.app[: options.app.rfind(os.sep)]
            if options.app
            else options.utilityPath
        )

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
                        if os.path.isdir(path) or (
                            os.path.isfile(path) and path.endswith(".xpi")
                        ):
                            extensions.append(path)
        extensions.extend(options.extensionsToInstall)
        return extensions

    def logPreamble(self, tests):
        """Logs a suite_start message and test_start/test_end at the beginning of a run."""
        self.log.suite_start(
            self.tests_by_manifest, name="mochitest-{}".format(self.flavor)
        )
        for test in tests:
            if "disabled" in test:
                self.log.test_start(test["path"])
                self.log.test_end(test["path"], "SKIP", message=test["disabled"])

    def loadFailurePatternFile(self, pat_file):
        if pat_file in self.patternFiles:
            return self.patternFiles[pat_file]
        if not os.path.isfile(pat_file):
            self.log.warning(
                "runtests.py | Cannot find failure pattern file " + pat_file
            )
            return None

        # Using ":error" to ensure it shows up in the failure summary.
        self.log.warning(
            "[runtests.py:error] Using {} to filter failures. If there "
            "is any number mismatch below, you could have fixed "
            "something documented in that file. Please reduce the "
            "failure count appropriately.".format(pat_file)
        )
        patternRE = re.compile(
            r"""
            ^\s*\*\s*               # list bullet
            (test_\S+|\.{3})        # test name
            (?:\s*(`.+?`|asserts))? # failure pattern
            (?::.+)?                # optional description
            \s*\[(\d+|\*)\]         # expected count
            \s*$
        """,
            re.X,
        )
        patterns = {}
        with open(pat_file) as f:
            last_name = None
            for line in f:
                match = patternRE.match(line)
                if not match:
                    continue
                name = match.group(1)
                name = last_name if name == "..." else name
                last_name = name
                pat = match.group(2)
                if pat is not None:
                    pat = "ASSERTION" if pat == "asserts" else pat[1:-1]
                count = match.group(3)
                count = None if count == "*" else int(count)
                if name not in patterns:
                    patterns[name] = []
                patterns[name].append((pat, count))
        self.patternFiles[pat_file] = patterns
        return patterns

    def getFailurePatterns(self, pat_file, test_name):
        patterns = self.loadFailurePatternFile(pat_file)
        if patterns:
            return patterns.get(test_name, None)

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

            filters = [
                subsuite(options.subsuite),
            ]

            if options.test_tags:
                filters.append(tags(options.test_tags))

            if options.test_paths:
                options.test_paths = self.normalize_paths(options.test_paths)
                filters.append(pathprefix(options.test_paths))

            # Add chunking filters if specified
            if options.totalChunks:
                if options.chunkByDir:
                    filters.append(
                        chunk_by_dir(
                            options.thisChunk, options.totalChunks, options.chunkByDir
                        )
                    )
                elif options.chunkByRuntime:
                    if mozinfo.info["os"] == "android":
                        platkey = "android"
                    elif mozinfo.isWin:
                        platkey = "windows"
                    else:
                        platkey = "unix"

                    runtime_file = os.path.join(
                        SCRIPT_DIR,
                        "runtimes",
                        "manifest-runtimes-{}.json".format(platkey),
                    )
                    if not os.path.exists(runtime_file):
                        self.log.error("runtime file %s not found!" % runtime_file)
                        sys.exit(1)

                    # Given the mochitest flavor, load the runtimes information
                    # for only that flavor due to manifest runtime format change in Bug 1637463.
                    with open(runtime_file, "r") as f:
                        if "suite_name" in options:
                            runtimes = json.load(f).get(options.suite_name, {})
                        else:
                            runtimes = {}

                    filters.append(
                        chunk_by_runtime(
                            options.thisChunk, options.totalChunks, runtimes
                        )
                    )
                else:
                    filters.append(
                        chunk_by_slice(options.thisChunk, options.totalChunks)
                    )

            noDefaultFilters = False
            if options.runFailures:
                filters.append(failures(options.runFailures))
                noDefaultFilters = True

            tests = manifest.active_tests(
                exists=False,
                disabled=disabled,
                filters=filters,
                noDefaultFilters=noDefaultFilters,
                **info
            )

            if len(tests) == 0:
                self.log.error(
                    NO_TESTS_FOUND.format(options.flavor, manifest.fmt_filters())
                )

        paths = []
        for test in tests:
            if len(tests) == 1 and "disabled" in test:
                del test["disabled"]

            pathAbs = os.path.abspath(test["path"])
            assert pathAbs.startswith(self.testRootAbs)
            tp = pathAbs[len(self.testRootAbs) :].replace("\\", "/").strip("/")

            if not self.isTest(options, tp):
                self.log.warning(
                    "Warning: %s from manifest %s is not a valid test"
                    % (test["name"], test["manifest"])
                )
                continue

            manifest_key = test["manifest_relpath"]
            # Ignore ancestor_manifests that live at the root (e.g, don't have a
            # path separator).
            if "ancestor_manifest" in test and "/" in normsep(
                test["ancestor_manifest"]
            ):
                manifest_key = "{}:{}".format(test["ancestor_manifest"], manifest_key)

            self.tests_by_manifest[manifest_key.replace("\\", "/")].append(tp)
            self.prefs_by_manifest[manifest_key].add(test.get("prefs"))
            self.env_vars_by_manifest[manifest_key].add(test.get("environment"))

            for key in ["prefs", "environment"]:
                if key in test and not options.runByManifest and "disabled" not in test:
                    self.log.error(
                        "parsing {}: runByManifest mode must be enabled to "
                        "set the `{}` key".format(test["manifest_relpath"], key)
                    )
                    sys.exit(1)

            testob = {"path": tp, "manifest": manifest_key}
            if "disabled" in test:
                testob["disabled"] = test["disabled"]
            if "expected" in test:
                testob["expected"] = test["expected"]
            if "scheme" in test:
                testob["scheme"] = test["scheme"]
            if "tags" in test:
                testob["tags"] = test["tags"]
            if options.failure_pattern_file:
                pat_file = os.path.join(
                    os.path.dirname(test["manifest"]), options.failure_pattern_file
                )
                patterns = self.getFailurePatterns(pat_file, test["name"])
                if patterns:
                    testob["expected"] = patterns
            paths.append(testob)

        # The 'prefs' key needs to be set in the DEFAULT section, unfortunately
        # we can't tell what comes from DEFAULT or not. So to validate this, we
        # stash all prefs from tests in the same manifest into a set. If the
        # length of the set > 1, then we know 'prefs' didn't come from DEFAULT.
        pref_not_default = [
            m for m, p in six.iteritems(self.prefs_by_manifest) if len(p) > 1
        ]
        if pref_not_default:
            self.log.error(
                "The 'prefs' key must be set in the DEFAULT section of a "
                "manifest. Fix the following manifests: {}".format(
                    "\n".join(pref_not_default)
                )
            )
            sys.exit(1)
        # The 'environment' key needs to be set in the DEFAULT section too.
        env_not_default = [
            m for m, p in six.iteritems(self.env_vars_by_manifest) if len(p) > 1
        ]
        if env_not_default:
            self.log.error(
                "The 'environment' key must be set in the DEFAULT section of a "
                "manifest. Fix the following manifests: {}".format(
                    "\n".join(env_not_default)
                )
            )
            sys.exit(1)

        paths.sort(key=lambda p: p["path"].split("/"))
        if options.dump_tests:
            options.dump_tests = os.path.expanduser(options.dump_tests)
            assert os.path.exists(os.path.dirname(options.dump_tests))
            with open(options.dump_tests, "w") as dumpFile:
                dumpFile.write(json.dumps({"active_tests": paths}))

            self.log.info("Dumping active_tests to %s file." % options.dump_tests)
            sys.exit()

        # Upload a list of test manifests that were executed in this run.
        if "MOZ_UPLOAD_DIR" in os.environ:
            artifact = os.path.join(os.environ["MOZ_UPLOAD_DIR"], "manifests.list")
            with open(artifact, "a") as fh:
                fh.write("\n".join(sorted(self.tests_by_manifest.keys())))

        self._active_tests = paths
        return self._active_tests

    def getTestManifest(self, options):
        if isinstance(options.manifestFile, TestManifest):
            manifest = options.manifestFile
        elif options.manifestFile and os.path.isfile(options.manifestFile):
            manifestFileAbs = os.path.abspath(options.manifestFile)
            assert manifestFileAbs.startswith(SCRIPT_DIR)
            manifest = TestManifest([options.manifestFile], strict=False)
        elif options.manifestFile and os.path.isfile(
            os.path.join(SCRIPT_DIR, options.manifestFile)
        ):
            manifestFileAbs = os.path.abspath(
                os.path.join(SCRIPT_DIR, options.manifestFile)
            )
            assert manifestFileAbs.startswith(SCRIPT_DIR)
            manifest = TestManifest([manifestFileAbs], strict=False)
        else:
            masterName = self.normflavor(options.flavor) + ".ini"
            masterPath = os.path.join(SCRIPT_DIR, self.testRoot, masterName)

            if os.path.exists(masterPath):
                manifest = TestManifest([masterPath], strict=False)
            else:
                manifest = None
                self.log.warning(
                    "TestManifest masterPath %s does not exist" % masterPath
                )

        return manifest

    def makeTestConfig(self, options):
        "Creates a test configuration file for customizing test execution."
        options.logFile = options.logFile.replace("\\", "\\\\")

        if (
            "MOZ_HIDE_RESULTS_TABLE" in os.environ
            and os.environ["MOZ_HIDE_RESULTS_TABLE"] == "1"
        ):
            options.hideResultsTable = True

        # strip certain unnecessary items to avoid serialization errors in json.dumps()
        d = dict(
            (k, v)
            for k, v in options.__dict__.items()
            if (v is None) or isinstance(v, (six.string_types, numbers.Number))
        )
        d["testRoot"] = self.testRoot
        if options.jscov_dir_prefix:
            d["jscovDirPrefix"] = options.jscov_dir_prefix
        if not options.keep_open:
            d["closeWhenDone"] = "1"

        d["runFailures"] = False
        if options.runFailures:
            d["runFailures"] = True
        content = json.dumps(d)

        with open(os.path.join(options.profilePath, "testConfig.js"), "w") as config:
            config.write(content)

    def buildBrowserEnv(self, options, debugger=False, env=None):
        """build the environment variables for the specific test and operating system"""
        if mozinfo.info["asan"] and mozinfo.isLinux and mozinfo.bits == 64:
            useLSan = True
        else:
            useLSan = False

        browserEnv = self.environment(
            xrePath=options.xrePath, env=env, debugger=debugger, useLSan=useLSan
        )

        if hasattr(options, "topsrcdir"):
            browserEnv["MOZ_DEVELOPER_REPO_DIR"] = options.topsrcdir
        if hasattr(options, "topobjdir"):
            browserEnv["MOZ_DEVELOPER_OBJ_DIR"] = options.topobjdir

        if options.headless:
            browserEnv["MOZ_HEADLESS"] = "1"

        if options.dmd:
            browserEnv["DMD"] = os.environ.get("DMD", "1")

        # bug 1443327: do not set MOZ_CRASHREPORTER_SHUTDOWN during browser-chrome
        # tests, since some browser-chrome tests test content process crashes;
        # also exclude non-e10s since at least one non-e10s mochitest is problematic
        if (
            options.flavor == "browser" or not options.e10s
        ) and "MOZ_CRASHREPORTER_SHUTDOWN" in browserEnv:
            del browserEnv["MOZ_CRASHREPORTER_SHUTDOWN"]

        try:
            browserEnv.update(
                dict(
                    parse_key_value(
                        self.extraEnv, context="environment variable in manifest"
                    )
                )
            )
        except KeyValueParseError as e:
            self.log.error(str(e))
            return None

        # These variables are necessary for correct application startup; change
        # via the commandline at your own risk.
        browserEnv["XPCOM_DEBUG_BREAK"] = "stack"

        # interpolate environment passed with options
        try:
            browserEnv.update(
                dict(parse_key_value(options.environment, context="--setenv"))
            )
        except KeyValueParseError as e:
            self.log.error(str(e))
            return None

        if (
            "MOZ_PROFILER_STARTUP_FEATURES" not in browserEnv
            or "nativeallocations"
            not in browserEnv["MOZ_PROFILER_STARTUP_FEATURES"].split(",")
        ):
            # Only turn on the bloat log if the profiler's native allocation feature is
            # not enabled. The two are not compatible.
            browserEnv["XPCOM_MEM_BLOAT_LOG"] = self.leak_report_file

        # If profiling options are enabled, turn on the gecko profiler by using the
        # profiler environmental variables.
        if options.profiler:
            # The user wants to capture a profile, and automatically view it. The
            # profile will be saved to a temporary folder, then deleted after
            # opening in profiler.firefox.com.
            self.profiler_tempdir = tempfile.mkdtemp()
            browserEnv["MOZ_PROFILER_SHUTDOWN"] = os.path.join(
                self.profiler_tempdir, "mochitest-profile.json"
            )
            browserEnv["MOZ_PROFILER_STARTUP"] = "1"

        if options.profilerSaveOnly:
            # The user wants to capture a profile, but only to save it. This defaults
            # to the MOZ_UPLOAD_DIR.
            browserEnv["MOZ_PROFILER_STARTUP"] = "1"
            if "MOZ_UPLOAD_DIR" in browserEnv:
                browserEnv["MOZ_PROFILER_SHUTDOWN"] = os.path.join(
                    browserEnv["MOZ_UPLOAD_DIR"], "mochitest-profile.json"
                )
            else:
                self.log.error(
                    "--profiler-save-only was specified, but no MOZ_UPLOAD_DIR "
                    "environment variable was provided. Please set this "
                    "environment variable to a directory path in order to save "
                    "a performance profile."
                )
                return None

        try:
            gmp_path = self.getGMPPluginPath(options)
            if gmp_path is not None:
                browserEnv["MOZ_GMP_PATH"] = gmp_path
        except EnvironmentError:
            self.log.error("Could not find path to gmp-fake plugin!")
            return None

        if options.fatalAssertions:
            browserEnv["XPCOM_DEBUG_BREAK"] = "stack-and-abort"

        # Produce a mozlog, if setup (see MOZ_LOG global at the top of
        # this script).
        self.mozLogs = MOZ_LOG and "MOZ_UPLOAD_DIR" in os.environ
        if self.mozLogs:
            browserEnv["MOZ_LOG"] = MOZ_LOG

        if options.enable_webrender:
            browserEnv["MOZ_WEBRENDER"] = "1"
            browserEnv["MOZ_ACCELERATED"] = "1"
        else:
            browserEnv["MOZ_WEBRENDER"] = "0"

        return browserEnv

    def killNamedProc(self, pname, orphans=True):
        """ Kill processes matching the given command name """
        self.log.info("Checking for %s processes..." % pname)

        if HAVE_PSUTIL:
            for proc in psutil.process_iter():
                try:
                    if proc.name() == pname:
                        procd = proc.as_dict(attrs=["pid", "ppid", "name", "username"])
                        if proc.ppid() == 1 or not orphans:
                            self.log.info("killing %s" % procd)
                            killPid(proc.pid, self.log)
                        else:
                            self.log.info("NOT killing %s (not an orphan?)" % procd)
                except Exception as e:
                    self.log.info(
                        "Warning: Unable to kill process %s: %s" % (pname, str(e))
                    )
                    # may not be able to access process info for all processes
                    continue
        else:

            def _psInfo(line):
                if pname in line:
                    self.log.info(line)

            process = mozprocess.ProcessHandler(
                ["ps", "-f"], processOutputLine=_psInfo, universal_newlines=True
            )
            process.run()
            process.wait()

            def _psKill(line):
                parts = line.split()
                if len(parts) == 3 and parts[0].isdigit():
                    pid = int(parts[0])
                    ppid = int(parts[1])
                    if parts[2] == pname:
                        if ppid == 1 or not orphans:
                            self.log.info("killing %s (pid %d)" % (pname, pid))
                            killPid(pid, self.log)
                        else:
                            self.log.info(
                                "NOT killing %s (pid %d) (not an orphan?)"
                                % (pname, pid)
                            )

            process = mozprocess.ProcessHandler(
                ["ps", "-o", "pid,ppid,comm"],
                processOutputLine=_psKill,
                universal_newlines=True,
            )
            process.run()
            process.wait()

    def execute_start_script(self):
        if not self.start_script or not self.marionette:
            return

        if os.path.isfile(self.start_script):
            with open(self.start_script, "r") as fh:
                script = fh.read()
        else:
            script = self.start_script

        with self.marionette.using_context("chrome"):
            return self.marionette.execute_script(
                script, script_args=(self.start_script_kwargs,)
            )

    def fillCertificateDB(self, options):
        # TODO: move -> mozprofile:
        # https://bugzilla.mozilla.org/show_bug.cgi?id=746243#c35

        pwfilePath = os.path.join(options.profilePath, ".crtdbpw")
        with open(pwfilePath, "w") as pwfile:
            pwfile.write("\n")

        # Pre-create the certification database for the profile
        env = self.environment(xrePath=options.xrePath)
        env["LD_LIBRARY_PATH"] = options.xrePath
        bin_suffix = mozinfo.info.get("bin_suffix", "")
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
            [certutil, "-N", "-d", certdbPath, "-f", pwfilePath], env=toolsEnv
        )
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
                call(
                    [
                        certutil,
                        "-A",
                        "-i",
                        os.path.join(options.certPath, item),
                        "-d",
                        certdbPath,
                        "-f",
                        pwfilePath,
                        "-n",
                        root,
                        "-t",
                        trustBits,
                    ],
                    env=toolsEnv,
                )
            elif ext == ".client":
                call(
                    [
                        pk12util,
                        "-i",
                        os.path.join(options.certPath, item),
                        "-w",
                        pwfilePath,
                        "-d",
                        certdbPath,
                    ],
                    env=toolsEnv,
                )

        os.unlink(pwfilePath)
        return 0

    def proxy(self, options):
        # proxy
        # use SSL port for legacy compatibility; see
        # - https://bugzilla.mozilla.org/show_bug.cgi?id=688667#c66
        # - https://bugzilla.mozilla.org/show_bug.cgi?id=899221
        # - https://github.com/mozilla/mozbase/commit/43f9510e3d58bfed32790c82a57edac5f928474d
        #             'ws': str(self.webSocketPort)
        return {
            "remote": options.webServer,
            "http": options.httpPort,
            "https": options.sslPort,
            "ws": options.sslPort,
        }

    def merge_base_profiles(self, options, category):
        """Merge extra profile data from testing/profiles."""

        # In test packages used in CI, the profile_data directory is installed
        # in the SCRIPT_DIR.
        profile_data_dir = os.path.join(SCRIPT_DIR, "profile_data")
        # If possible, read profile data from topsrcdir. This prevents us from
        # requiring a re-build to pick up newly added extensions in the
        # <profile>/extensions directory.
        if build_obj:
            path = os.path.join(build_obj.topsrcdir, "testing", "profiles")
            if os.path.isdir(path):
                profile_data_dir = path
        # Still not found? Look for testing/profiles relative to testing/mochitest.
        if not os.path.isdir(profile_data_dir):
            path = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "profiles"))
            if os.path.isdir(path):
                profile_data_dir = path

        with open(os.path.join(profile_data_dir, "profiles.json"), "r") as fh:
            base_profiles = json.load(fh)[category]

        # values to use when interpolating preferences
        interpolation = {
            "server": "%s:%s" % (options.webServer, options.httpPort),
        }

        for profile in base_profiles:
            path = os.path.join(profile_data_dir, profile)
            self.profile.merge(path, interpolation=interpolation)

    def buildProfile(self, options):
        """ create the profile and add optional chrome bits and files if requested """
        # get extensions to install
        extensions = self.getExtensionsToInstall(options)

        # Whitelist the _tests directory (../..) so that TESTING_JS_MODULES work
        tests_dir = os.path.dirname(os.path.dirname(SCRIPT_DIR))
        sandbox_whitelist_paths = [tests_dir] + options.sandboxReadWhitelist
        if platform.system() == "Linux" or platform.system() in (
            "Windows",
            "Microsoft",
        ):
            # Trailing slashes are needed to indicate directories on Linux and Windows
            sandbox_whitelist_paths = [
                os.path.join(p, "") for p in sandbox_whitelist_paths
            ]

        # Create the profile
        self.profile = Profile(
            profile=options.profilePath,
            addons=extensions,
            locations=self.locations,
            proxy=self.proxy(options),
            whitelistpaths=sandbox_whitelist_paths,
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
                "TEST-UNEXPECTED-FAIL | runtests.py | Certificate integration failed"
            )
            return None

        # Set preferences in the following order (latter overrides former):
        # 1) Preferences from base profile (e.g from testing/profiles)
        # 2) Prefs hardcoded in this function
        # 3) Prefs from --setpref

        # Prefs from base profiles
        self.merge_base_profiles(options, "mochitest")

        # Hardcoded prefs (TODO move these into a base profile)
        prefs = {
            "browser.tabs.remote.autostart": options.e10s,
            # Enable tracing output for detailed failures in case of
            # failing connection attempts, and hangs (bug 1397201)
            "marionette.log.level": "Trace",
            # Disable async font fallback, because the unpredictable
            # extra reflow it can trigger (potentially affecting a later
            # test) results in spurious intermittent failures.
            "gfx.font_rendering.fallback.async": False,
        }

        # Ideally we should set this in a manifest, but a11y tests do not run by manifest.
        if options.flavor == "a11y":
            prefs["plugin.load_flash_only"] = False

        if options.flavor == "browser" and options.timeout:
            prefs["testing.browserTestHarness.timeout"] = options.timeout

        # browser-chrome tests use a fairly short default timeout of 45 seconds;
        # this is sometimes too short on asan and debug, where we expect reduced
        # performance.
        if (
            (mozinfo.info["asan"] or mozinfo.info["debug"])
            and options.flavor == "browser"
            and options.timeout is None
        ):
            self.log.info("Increasing default timeout to 90 seconds")
            prefs["testing.browserTestHarness.timeout"] = 90

        # tsan builds need even more time
        if (
            mozinfo.info["tsan"]
            and options.flavor == "browser"
            and options.timeout is None
        ):
            self.log.info("Increasing default timeout to 120 seconds")
            prefs["testing.browserTestHarness.timeout"] = 120

        if mozinfo.info["os"] == "win" and mozinfo.info["processor"] == "aarch64":
            extended_timeout = self.DEFAULT_TIMEOUT * 4
            self.log.info(
                "Increasing default timeout to {} seconds".format(extended_timeout)
            )
            prefs["testing.browserTestHarness.timeout"] = extended_timeout

        if getattr(self, "testRootAbs", None):
            prefs["mochitest.testRoot"] = self.testRootAbs

        # See if we should use fake media devices.
        if options.useTestMediaDevices:
            prefs["media.audio_loopback_dev"] = self.mediaDevices["audio"]
            prefs["media.video_loopback_dev"] = self.mediaDevices["video"]
            prefs["media.cubeb.output_device"] = "Null Output"
            prefs["media.volume_scale"] = "1.0"

        self.profile.set_preferences(prefs)

        # Extra prefs from --setpref
        self.profile.set_preferences(self.extraPrefs)
        return manifest

    def getGMPPluginPath(self, options):
        if options.gmp_path:
            return options.gmp_path

        gmp_parentdirs = [
            # For local builds, GMP plugins will be under dist/bin.
            options.xrePath,
            # For packaged builds, GMP plugins will get copied under
            # $profile/plugins.
            os.path.join(self.profile.profile, "plugins"),
        ]

        gmp_subdirs = [
            os.path.join("gmp-fake", "1.0"),
            os.path.join("gmp-fakeopenh264", "1.0"),
            os.path.join("gmp-clearkey", "0.1"),
        ]

        gmp_paths = [
            os.path.join(parent, sub)
            for parent in gmp_parentdirs
            for sub in gmp_subdirs
            if os.path.isdir(os.path.join(parent, sub))
        ]

        if not gmp_paths:
            # This is fatal for desktop environments.
            raise EnvironmentError("Could not find test gmp plugins")

        return os.pathsep.join(gmp_paths)

    def cleanup(self, options, final=False):
        """ remove temporary files and profile """
        if hasattr(self, "manifest") and self.manifest is not None:
            if os.path.exists(self.manifest):
                os.remove(self.manifest)
        if hasattr(self, "profile"):
            del self.profile
        if options.pidFile != "" and os.path.exists(options.pidFile):
            try:
                os.remove(options.pidFile)
                if os.path.exists(options.pidFile + ".xpcshell.pid"):
                    os.remove(options.pidFile + ".xpcshell.pid")
            except Exception:
                self.log.warning(
                    "cleaning up pidfile '%s' was unsuccessful from the test harness"
                    % options.pidFile
                )
        options.manifestFile = None

    def dumpScreen(self, utilityPath):
        if self.haveDumpedScreen:
            self.log.info(
                "Not taking screenshot here: see the one that was previously logged"
            )
            return
        self.haveDumpedScreen = True
        dump_screen(utilityPath, self.log)

    def killAndGetStack(self, processPID, utilityPath, debuggerInfo, dump_screen=False):
        """
        Kill the process, preferrably in a way that gets us a stack trace.
        Also attempts to obtain a screenshot before killing the process
        if specified.
        """
        self.log.info("Killing process: %s" % processPID)
        if dump_screen:
            self.dumpScreen(utilityPath)

        if mozinfo.info.get("crashreporter", True) and not debuggerInfo:
            try:
                minidump_path = os.path.join(self.profile.profile, "minidumps")
                mozcrash.kill_and_get_minidump(processPID, minidump_path, utilityPath)
            except OSError:
                # https://bugzilla.mozilla.org/show_bug.cgi?id=921509
                self.log.info("Can't trigger Breakpad, process no longer exists")
            return
        self.log.info("Can't trigger Breakpad, just killing process")
        killPid(processPID, self.log)

    def extract_child_pids(self, process_log, parent_pid=None):
        """Parses the given log file for the pids of any processes launched by
        the main process and returns them as a list.
        If parent_pid is provided, and psutil is available, returns children of
        parent_pid according to psutil.
        """
        rv = []
        if parent_pid and HAVE_PSUTIL:
            self.log.info("Determining child pids from psutil...")
            try:
                rv = [p.pid for p in psutil.Process(parent_pid).children()]
                self.log.info(str(rv))
            except psutil.NoSuchProcess:
                self.log.warning("Failed to lookup children of pid %d" % parent_pid)

        rv = set(rv)
        pid_re = re.compile(r"==> process \d+ launched child process (\d+)")
        with open(process_log) as fd:
            for line in fd:
                self.log.info(line.rstrip())
                m = pid_re.search(line)
                if m:
                    rv.add(int(m.group(1)))
        return rv

    def checkForZombies(self, processLog, utilityPath, debuggerInfo):
        """Look for hung processes"""

        if not os.path.exists(processLog):
            self.log.info("Automation Error: PID log not found: %s" % processLog)
            # Whilst no hung process was found, the run should still display as
            # a failure
            return True

        # scan processLog for zombies
        self.log.info("zombiecheck | Reading PID log: %s" % processLog)
        processList = self.extract_child_pids(processLog)
        # kill zombies
        foundZombie = False
        for processPID in processList:
            self.log.info(
                "zombiecheck | Checking for orphan process with PID: %d" % processPID
            )
            if isPidAlive(processPID):
                foundZombie = True
                self.log.error(
                    "TEST-UNEXPECTED-FAIL | zombiecheck | child process "
                    "%d still alive after shutdown" % processPID
                )
                self.killAndGetStack(
                    processPID, utilityPath, debuggerInfo, dump_screen=not debuggerInfo
                )

        return foundZombie

    def checkForRunningBrowsers(self):
        firefoxes = ""
        if HAVE_PSUTIL:
            attrs = ["pid", "ppid", "name", "cmdline", "username"]
            for proc in psutil.process_iter():
                try:
                    if "firefox" in proc.name():
                        firefoxes = "%s%s\n" % (firefoxes, proc.as_dict(attrs=attrs))
                except Exception:
                    # may not be able to access process info for all processes
                    continue
        if len(firefoxes) > 0:
            # In automation, this warning is unexpected and should be investigated.
            # In local testing, this is probably okay, as long as the browser is not
            # running a marionette server.
            self.log.warning("Found 'firefox' running before starting test browser!")
            self.log.warning(firefoxes)

    def runApp(
        self,
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
        marionette_args=None,
        e10s=True,
        runFailures=False,
        crashAsPass=False,
    ):
        """
        Run the app, log the duration it took to execute, return the status code.
        Kills the app if it runs for longer than |maxTime| seconds, or outputs nothing
        for |timeout| seconds.
        """
        # It can't be the case that both a with-debugger and an
        # on-Valgrind run have been requested.  doTests() should have
        # already excluded this possibility.
        assert not (valgrindPath and debuggerInfo)

        # debugger information
        interactive = False
        debug_args = None
        if debuggerInfo:
            interactive = debuggerInfo.interactive
            debug_args = [debuggerInfo.path] + debuggerInfo.args

        # Set up Valgrind arguments.
        if valgrindPath:
            interactive = False
            valgrindArgs_split = (
                [] if valgrindArgs is None else shlex.split(valgrindArgs)
            )

            valgrindSuppFiles_final = []
            if valgrindSuppFiles is not None:
                valgrindSuppFiles_final = [
                    "--suppressions=" + path for path in valgrindSuppFiles.split(",")
                ]

            debug_args = (
                [valgrindPath]
                + mozdebug.get_default_valgrind_args()
                + valgrindArgs_split
                + valgrindSuppFiles_final
            )

        # fix default timeout
        if timeout == -1:
            timeout = self.DEFAULT_TIMEOUT

        # Note in the log if running on Valgrind
        if valgrindPath:
            self.log.info(
                "runtests.py | Running on Valgrind.  "
                + "Using timeout of %d seconds." % timeout
            )

        # copy env so we don't munge the caller's environment
        env = env.copy()

        # Used to defer a possible IOError exception from Marionette
        marionette_exception = None

        temp_file_paths = []

        # make sure we clean up after ourselves.
        try:
            # set process log environment variable
            tmpfd, processLog = tempfile.mkstemp(suffix="pidlog")
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
            args.append("-marionette")
            # TODO: mozrunner should use -foreground at least for mac
            # https://bugzilla.mozilla.org/show_bug.cgi?id=916512
            args.append("-foreground")
            self.start_script_kwargs["testUrl"] = testUrl or "about:blank"

            if detectShutdownLeaks:
                env["MOZ_LOG"] = (
                    env["MOZ_LOG"] + "," if env["MOZ_LOG"] else ""
                ) + "DocShellAndDOMWindowLeak:3"
                shutdownLeaks = ShutdownLeaks(self.log)
            else:
                shutdownLeaks = None

            if mozinfo.info["asan"] and mozinfo.isLinux and mozinfo.bits == 64:
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
                bisectChunk=bisectChunk,
            )

            def timeoutHandler():
                browserProcessId = outputHandler.browserProcessId
                self.handleTimeout(
                    timeout,
                    proc,
                    utilityPath,
                    debuggerInfo,
                    browserProcessId,
                    processLog,
                )

            kp_kwargs = {
                "kill_on_timeout": False,
                "cwd": SCRIPT_DIR,
                "onTimeout": [timeoutHandler],
            }
            kp_kwargs["processOutputLine"] = [outputHandler]

            self.checkForRunningBrowsers()

            # create mozrunner instance and start the system under test process
            self.lastTestSeen = self.test_name
            startTime = datetime.now()

            runner_cls = mozrunner.runners.get(
                mozinfo.info.get("appname", "firefox"), mozrunner.Runner
            )
            runner = runner_cls(
                profile=self.profile,
                binary=cmd,
                cmdargs=args,
                env=env,
                process_class=mozprocess.ProcessHandlerMixin,
                process_args=kp_kwargs,
            )

            # start the runner
            runner.start(
                debug_args=debug_args, interactive=interactive, outputTimeout=timeout
            )
            proc = runner.process_handler
            self.log.info("runtests.py | Application pid: %d" % proc.pid)

            gecko_id = "GECKO(%d)" % proc.pid
            self.log.process_start(gecko_id)
            self.message_logger.gecko_id = gecko_id

            try:
                # start marionette and kick off the tests
                marionette_args = marionette_args or {}
                self.marionette = Marionette(**marionette_args)
                self.marionette.start_session()

                # install specialpowers and mochikit addons
                addons = Addons(self.marionette)

                if self.staged_addons:
                    for addon_path in self.staged_addons:
                        if not os.path.isdir(addon_path):
                            self.log.error(
                                "TEST-UNEXPECTED-FAIL | invalid setup: missing extension at %s"
                                % addon_path
                            )
                            return 1, self.lastTestSeen
                        temp_addon_path = create_zip(addon_path)
                        temp_file_paths.append(temp_addon_path)
                        addons.install(temp_addon_path)

                self.execute_start_script()

                # an open marionette session interacts badly with mochitest,
                # delete it until we figure out why.
                self.marionette.delete_session()
                del self.marionette

            except IOError:
                # Any IOError as thrown by Marionette means that something is
                # wrong with the process, like a crash or the socket is no
                # longer open. We defer raising this specific error so that
                # post-test checks for leaks and crashes are performed and
                # reported first.
                marionette_exception = sys.exc_info()

            # wait until app is finished
            # XXX copy functionality from
            # https://github.com/mozilla/mozbase/blob/master/mozrunner/mozrunner/runner.py#L61
            # until bug 913970 is fixed regarding mozrunner `wait` not returning status
            # see https://bugzilla.mozilla.org/show_bug.cgi?id=913970
            self.log.info("runtests.py | Waiting for browser...")
            status = proc.wait()
            if status is None:
                self.log.warning(
                    "runtests.py | Failed to get app exit code - running/crashed?"
                )
                # must report an integer to process_exit()
                status = 0
            self.log.process_exit("Main app process", status)
            runner.process_handler = None

            # finalize output handler
            outputHandler.finish()

            # record post-test information
            if status:
                self.message_logger.dump_buffered()
                msg = ("application terminated with exit code %s" % status,)
                # self.message_logger.is_test_running indicates we need to send a test_end
                if crashAsPass and self.message_logger.is_test_running:
                    # this works for browser-chrome, mochitest-plain has status=0
                    message = {
                        "action": "test_end",
                        "status": "CRASH",
                        "expected": "CRASH",
                        "thread": None,
                        "pid": None,
                        "source": "mochitest",
                        "time": int(time.time()) * 1000,
                        "test": self.lastTestSeen,
                        "message": msg,
                    }
                    # need to send a test_end in order to have mozharness process messages properly
                    # this requires a custom message vs log.error/log.warning/etc.
                    self.message_logger.process_message(message)
                else:
                    self.log.error(msg)
            else:
                self.lastTestSeen = "Main app process exited normally"

            self.log.info(
                "runtests.py | Application ran for: %s"
                % str(datetime.now() - startTime)
            )

            # Do a final check for zombie child processes.
            zombieProcesses = self.checkForZombies(
                processLog, utilityPath, debuggerInfo
            )

            # check for crashes
            quiet = False
            if crashAsPass:
                quiet = True

            minidump_path = os.path.join(self.profile.profile, "minidumps")
            crash_count = mozcrash.log_crashes(
                self.log,
                minidump_path,
                symbolsPath,
                test=self.lastTestSeen,
                quiet=quiet,
            )

            if crashAsPass:
                # self.message_logger.is_test_running indicates we need a test_end message
                if crash_count > 0 and self.message_logger.is_test_running:
                    # this works for browser-chrome, mochitest-plain has status=0
                    message = {
                        "action": "test_end",
                        "status": "CRASH",
                        "expected": "CRASH",
                        "thread": None,
                        "pid": None,
                        "source": "mochitest",
                        "time": int(time.time()) * 1000,
                        "test": self.lastTestSeen,
                        "message": "application terminated with exit code 0",
                    }
                    # need to send a test_end in order to have mozharness process messages properly
                    # this requires a custom message vs log.error/log.warning/etc.
                    self.message_logger.process_message(message)
                status = 0
            elif crash_count or zombieProcesses:
                status = 1
        finally:
            # cleanup
            if os.path.exists(processLog):
                os.remove(processLog)
            for p in temp_file_paths:
                os.remove(p)

        if marionette_exception is not None:
            exc, value, tb = marionette_exception
            six.reraise(exc, value, tb)

        return status, self.lastTestSeen

    def initializeLooping(self, options):
        """
        This method is used to clear the contents before each run of for loop.
        This method is used for --run-by-dir and --bisect-chunk.
        """
        self.expectedError.clear()
        self.result.clear()
        options.manifestFile = None
        options.profilePath = None

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
                    self.log.error(
                        "TEST-UNEXPECTED-FAIL | Bisection | Please ignore repeats "
                        "and look for 'Bleedthrough' (if any) at the end of "
                        "the failure list"
                    )
                    bisection_log = 1

            result = self.doTests(options, testsToRun)
            if options.bisectChunk:
                status = bisect.post_test(options, self.expectedError, self.result)
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

    def groupTestsByScheme(self, tests):
        """
        split tests into groups by schemes. test is classified as http if
        no scheme specified
        """
        httpTests = []
        httpsTests = []
        for test in tests:
            if not test.get("scheme") or test.get("scheme") == "http":
                httpTests.append(test)
            elif test.get("scheme") == "https":
                httpsTests.append(test)
        return {"http": httpTests, "https": httpsTests}

    def verifyTests(self, options):
        """
        Support --verify mode: Run test(s) many times in a variety of
        configurations/environments in an effort to find intermittent
        failures.
        """

        # Number of times to repeat test(s) when running with --repeat
        VERIFY_REPEAT = 10
        # Number of times to repeat test(s) when running test in
        VERIFY_REPEAT_SINGLE_BROWSER = 5

        def step1():
            stepOptions = copy.deepcopy(options)
            stepOptions.repeat = VERIFY_REPEAT
            stepOptions.keep_open = False
            stepOptions.runUntilFailure = True
            stepOptions.profilePath = None
            result = self.runTests(stepOptions)
            result = result or (-2 if self.countfail > 0 else 0)
            self.message_logger.finish()
            return result

        def step2():
            stepOptions = copy.deepcopy(options)
            stepOptions.repeat = 0
            stepOptions.keep_open = False
            for i in range(VERIFY_REPEAT_SINGLE_BROWSER):
                stepOptions.profilePath = None
                result = self.runTests(stepOptions)
                result = result or (-2 if self.countfail > 0 else 0)
                self.message_logger.finish()
                if result != 0:
                    break
            return result

        def step3():
            stepOptions = copy.deepcopy(options)
            stepOptions.repeat = VERIFY_REPEAT
            stepOptions.keep_open = False
            stepOptions.environment.append("MOZ_CHAOSMODE=0xfb")
            stepOptions.profilePath = None
            result = self.runTests(stepOptions)
            result = result or (-2 if self.countfail > 0 else 0)
            self.message_logger.finish()
            return result

        def step4():
            stepOptions = copy.deepcopy(options)
            stepOptions.repeat = 0
            stepOptions.keep_open = False
            stepOptions.environment.append("MOZ_CHAOSMODE=0xfb")
            for i in range(VERIFY_REPEAT_SINGLE_BROWSER):
                stepOptions.profilePath = None
                result = self.runTests(stepOptions)
                result = result or (-2 if self.countfail > 0 else 0)
                self.message_logger.finish()
                if result != 0:
                    break
            return result

        def fission_step(fission_pref):
            stepOptions = copy.deepcopy(options)
            stepOptions.extraPrefs.append(fission_pref)
            stepOptions.keep_open = False
            stepOptions.runUntilFailure = True
            stepOptions.profilePath = None
            result = self.runTests(stepOptions)
            result = result or (-2 if self.countfail > 0 else 0)
            self.message_logger.finish()
            return result

        def fission_step1():
            return fission_step("fission.autostart=false")

        def fission_step2():
            return fission_step("fission.autostart=true")

        if options.verify_fission:
            steps = [
                ("1. Run each test without fission.", fission_step1),
                ("2. Run each test with fission.", fission_step2),
            ]
        else:
            steps = [
                ("1. Run each test %d times in one browser." % VERIFY_REPEAT, step1),
                (
                    "2. Run each test %d times in a new browser each time."
                    % VERIFY_REPEAT_SINGLE_BROWSER,
                    step2,
                ),
                (
                    "3. Run each test %d times in one browser, in chaos mode."
                    % VERIFY_REPEAT,
                    step3,
                ),
                (
                    "4. Run each test %d times in a new browser each time, "
                    "in chaos mode." % VERIFY_REPEAT_SINGLE_BROWSER,
                    step4,
                ),
            ]

        stepResults = {}
        for (descr, step) in steps:
            stepResults[descr] = "not run / incomplete"

        startTime = datetime.now()
        maxTime = timedelta(seconds=options.verify_max_time)
        finalResult = "PASSED"
        for (descr, step) in steps:
            if (datetime.now() - startTime) > maxTime:
                self.log.info("::: Test verification is taking too long: Giving up!")
                self.log.info(
                    "::: So far, all checks passed, but not all checks were run."
                )
                break
            self.log.info(":::")
            self.log.info('::: Running test verification step "%s"...' % descr)
            self.log.info(":::")
            result = step()
            if result != 0:
                stepResults[descr] = "FAIL"
                finalResult = "FAILED!"
                break
            stepResults[descr] = "Pass"

        self.logPreamble([])

        self.log.info(":::")
        self.log.info("::: Test verification summary for:")
        self.log.info(":::")
        tests = self.getActiveTests(options)
        for test in tests:
            self.log.info("::: " + test["path"])
        self.log.info(":::")
        for descr in sorted(stepResults.keys()):
            self.log.info("::: %s : %s" % (descr, stepResults[descr]))
        self.log.info(":::")
        self.log.info("::: Test verification %s" % finalResult)
        self.log.info(":::")

        return 0

    def runTests(self, options):
        """ Prepare, configure, run tests and cleanup """
        self.extraPrefs = parse_preferences(options.extraPrefs)

        # for test manifest parsing.
        mozinfo.update(
            {
                "a11y_checks": options.a11y_checks,
                "e10s": options.e10s,
                "fission": self.extraPrefs.get("fission.autostart", False),
                "headless": options.headless,
                # Until the test harness can understand default pref values,
                # (https://bugzilla.mozilla.org/show_bug.cgi?id=1577912) this value
                # should by synchronized with the default pref value indicated in
                # StaticPrefList.yaml.
                #
                # Currently for automation, the pref defaults to true (but can be
                # overridden with --setpref).
                "serviceworker_e10s": self.extraPrefs.get(
                    "dom.serviceWorkers.parent_intercept", True
                ),
                "sessionHistoryInParent": self.extraPrefs.get(
                    "fission.sessionHistoryInParent", False
                )
                or self.extraPrefs.get("fission.autostart", False),
                "socketprocess_e10s": self.extraPrefs.get(
                    "network.process.enabled", False
                ),
                "socketprocess_networking": self.extraPrefs.get(
                    "network.http.network_access_on_socket_process.enabled", False
                ),
                "swgl": self.extraPrefs.get("gfx.webrender.software", False),
                "verify": options.verify,
                "verify_fission": options.verify_fission,
                "webrender": options.enable_webrender,
                "xorigin": options.xOriginTests,
            }
        )

        self.setTestRoot(options)

        # Despite our efforts to clean up servers started by this script, in practice
        # we still see infrequent cases where a process is orphaned and interferes
        # with future tests, typically because the old server is keeping the port in use.
        # Try to avoid those failures by checking for and killing servers before
        # trying to start new ones.
        self.killNamedProc("ssltunnel")
        self.killNamedProc("xpcshell")

        if options.cleanupCrashes:
            mozcrash.cleanup_pending_crash_reports()

        tests = self.getActiveTests(options)
        self.logPreamble(tests)

        if mozinfo.info["fission"] and not mozinfo.info["e10s"]:
            # Make sure this is logged *after* suite_start so it gets associated with the
            # current suite in the summary formatters.
            self.log.error("Fission is not supported without e10s.")
            return 1

        tests = [t for t in tests if "disabled" not in t]

        # Until we have all green, this does not run on a11y (for perf reasons)
        if not options.runByManifest:
            return self.runMochitests(options, [t["path"] for t in tests])

        # code for --run-by-manifest
        manifests = set(t["manifest"] for t in tests)
        result = 0

        origPrefs = self.extraPrefs.copy()
        for m in sorted(manifests):
            self.log.info("Running manifest: {}".format(m))

            prefs = list(self.prefs_by_manifest[m])[0]
            self.extraPrefs = origPrefs.copy()
            if prefs:
                prefs = prefs.strip().split()
                self.log.info(
                    "The following extra prefs will be set:\n  {}".format(
                        "\n  ".join(prefs)
                    )
                )
                self.extraPrefs.update(parse_preferences(prefs))

            envVars = list(self.env_vars_by_manifest[m])[0]
            self.extraEnv = {}
            if envVars:
                self.extraEnv = envVars.strip().split()
                self.log.info(
                    "The following extra environment variables will be set:\n  {}".format(
                        "\n  ".join(self.extraEnv)
                    )
                )

            # If we are using --run-by-manifest, we should not use the profile path (if) provided
            # by the user, since we need to create a new directory for each run. We would face
            # problems if we use the directory provided by the user.
            tests_in_manifest = [t["path"] for t in tests if t["manifest"] == m]
            res = self.runMochitests(options, tests_in_manifest)
            result = result or res

            # Dump the logging buffer
            self.message_logger.dump_buffered()

            if res == -1:
                break

        if self.manifest is not None:
            self.cleanup(options, True)

        e10s_mode = "e10s" if options.e10s else "non-e10s"

        # for failure mode: where browser window has crashed and we have no reported results
        if (
            self.countpass == self.countfail == self.counttodo == 0
            and options.crashAsPass
        ):
            self.countpass = 1
            self.result = 0

        # printing total number of tests
        if options.flavor == "browser":
            print("TEST-INFO | checking window state")
            print("Browser Chrome Test Summary")
            print("\tPassed: %s" % self.countpass)
            print("\tFailed: %s" % self.countfail)
            print("\tTodo: %s" % self.counttodo)
            print("\tMode: %s" % e10s_mode)
            print("*** End BrowserChrome Test Results ***")
        else:
            print("0 INFO TEST-START | Shutdown")
            print("1 INFO Passed:  %s" % self.countpass)
            print("2 INFO Failed:  %s" % self.countfail)
            print("3 INFO Todo:    %s" % self.counttodo)
            print("4 INFO Mode:    %s" % e10s_mode)
            print("5 INFO SimpleTest FINISHED")

        # If shutdown profiling was enabled, then the user will want to access the
        # performance profile. The following code with display helpful log messages
        # and automatically open the profile if it is requested.
        if self.browserEnv and "MOZ_PROFILER_SHUTDOWN" in self.browserEnv:
            profile_path = self.browserEnv["MOZ_PROFILER_SHUTDOWN"]

            profiler_logger = get_proxy_logger("profiler")
            profiler_logger.info("Shutdown performance profiling was enabled")
            profiler_logger.info("Profile saved locally to: %s" % profile_path)

            if options.profilerSaveOnly or options.profiler:
                # Only do the extra work of symbolicating and viewing the profile if
                # officially requested through a command line flag. The MOZ_PROFILER_*
                # flags can be set by a user.
                symbolicate_profile_json(profile_path, options.topobjdir)
                view_gecko_profile_from_mochitest(
                    profile_path, options, profiler_logger
                )
            else:
                profiler_logger.info(
                    "The profiler was enabled outside of the mochitests. "
                    "Use --profiler instead of MOZ_PROFILER_SHUTDOWN to "
                    "symbolicate and open the profile automatically."
                )

            # Clean up the temporary file if it exists.
            if self.profiler_tempdir:
                shutil.rmtree(self.profiler_tempdir)

        if not result:
            if self.countfail or not (self.countpass or self.counttodo):
                # at least one test failed, or
                # no tests passed, and no tests failed (possibly a crash)
                result = 1

        return result

    def doTests(self, options, testsToFilter=None):
        # A call to initializeLooping method is required in case of --run-by-dir or --bisect-chunk
        # since we need to initialize variables for each loop.
        if options.bisectChunk or options.runByManifest:
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
                options.debugger, options.debuggerArgs, options.debuggerInteractive
            )

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
            self.log.error(
                "Specified --valgrind-args or --valgrind-supp-files,"
                " but not --valgrind"
            )
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

        self.leak_report_file = os.path.join(options.profilePath, "runtests_leaks.log")

        self.browserEnv = self.buildBrowserEnv(options, debuggerInfo is not None)

        if self.browserEnv is None:
            return 1

        if self.mozLogs:
            self.browserEnv["MOZ_LOG_FILE"] = "{}/moz-pid=%PID-uid={}.log".format(
                self.browserEnv["MOZ_UPLOAD_DIR"], str(uuid.uuid4())
            )

        status = 0
        try:
            self.startServers(options, debuggerInfo)

            if options.jsconsole:
                options.browserArgs.extend(["--jsconsole"])

            if options.jsdebugger:
                options.browserArgs.extend(["-wait-for-jsdebugger", "-jsdebugger"])

            # -jsdebugger takes a binary path as an optional argument.
            # Append jsdebuggerPath right after `-jsdebugger`.
            if options.jsdebuggerPath:
                options.browserArgs.extend([options.jsdebuggerPath])

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
                # We generally want the JS harness or marionette to handle
                # timeouts if they can.
                # The default JS harness timeout is currently 300 seconds.
                # The default Marionette socket timeout is currently 360 seconds.
                # Wait a little (10 seconds) more before timing out here.
                # See bug 479518 and bug 1414063.
                timeout = 370.0

            # Detect shutdown leaks for m-bc runs if
            # code coverage is not enabled.
            detectShutdownLeaks = False
            if options.jscov_dir_prefix is None:
                detectShutdownLeaks = (
                    mozinfo.info["debug"]
                    and options.flavor == "browser"
                    and options.subsuite != "thunderbird"
                    and not options.crashAsPass
                )

            self.start_script_kwargs["flavor"] = self.normflavor(options.flavor)
            marionette_args = {
                "symbols_path": options.symbolsPath,
                "socket_timeout": options.marionette_socket_timeout,
                "startup_timeout": options.marionette_startup_timeout,
            }

            if options.marionette:
                host, port = options.marionette.split(":")
                marionette_args["host"] = host
                marionette_args["port"] = int(port)

            # testsToFilter parameter is used to filter out the test list that
            # is sent to getTestsByScheme
            for (scheme, tests) in self.getTestsByScheme(options, testsToFilter):
                # read the number of tests here, if we are not going to run any,
                # terminate early
                if not tests:
                    continue

                self.currentTests = [t["path"] for t in tests]
                testURL = self.buildTestURL(options, scheme=scheme)

                self.buildURLOptions(options, self.browserEnv)
                if self.urlOpts:
                    testURL += "?" + "&".join(self.urlOpts)

                if options.runFailures:
                    testURL += "&runFailures=true"

                if options.timeoutAsPass:
                    testURL += "&timeoutAsPass=true"

                self.log.info("runtests.py | Running with scheme: {}".format(scheme))
                self.log.info(
                    "runtests.py | Running with e10s: {}".format(options.e10s)
                )
                self.log.info(
                    "runtests.py | Running with fission: {}".format(
                        mozinfo.info.get("fission", False)
                    )
                )
                self.log.info(
                    "runtests.py | Running with cross-origin iframes: {}".format(
                        mozinfo.info.get("xorigin", False)
                    )
                )
                self.log.info(
                    "runtests.py | Running with serviceworker_e10s: {}".format(
                        mozinfo.info.get("serviceworker_e10s", False)
                    )
                )
                self.log.info(
                    "runtests.py | Running with socketprocess_e10s: {}".format(
                        mozinfo.info.get("socketprocess_e10s", False)
                    )
                )
                self.log.info("runtests.py | Running tests: start.\n")
                ret, _ = self.runApp(
                    testURL,
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
                    e10s=options.e10s,
                    runFailures=options.runFailures,
                    crashAsPass=options.crashAsPass,
                )
                status = ret or status
        except KeyboardInterrupt:
            self.log.info("runtests.py | Received keyboard interrupt.\n")
            status = -1
        except Exception as e:
            traceback.print_exc()
            self.log.error(
                "Automation Error: Received unexpected exception while running application\n"
            )
            if "ADBTimeoutError" in repr(e):
                self.log.info("runtests.py | Device disconnected. Aborting test.\n")
                raise
            status = 1
        finally:
            self.stopServers()

        ignoreMissingLeaks = options.ignoreMissingLeaks
        leakThresholds = options.leakThresholds

        if options.crashAsPass:
            ignoreMissingLeaks.append("tab")
            ignoreMissingLeaks.append("socket")

        # Stop leak detection if m-bc code coverage is enabled
        # by maxing out the leak threshold for all processes.
        if options.jscov_dir_prefix:
            for processType in leakThresholds:
                ignoreMissingLeaks.append(processType)
                leakThresholds[processType] = sys.maxsize

        utilityPath = options.utilityPath or options.xrePath
        mozleak.process_leak_log(
            self.leak_report_file,
            leak_thresholds=leakThresholds,
            ignore_missing_leaks=ignoreMissingLeaks,
            log=self.log,
            stack_fixer=get_stack_fixer_function(utilityPath, options.symbolsPath),
        )

        self.log.info("runtests.py | Running tests: end.")

        if self.manifest is not None:
            self.cleanup(options, False)

        return status

    def handleTimeout(
        self, timeout, proc, utilityPath, debuggerInfo, browser_pid, processLog
    ):
        """handle process output timeout"""
        # TODO: bug 913975 : _processOutput should call self.processOutputLine
        # one more time one timeout (I think)
        error_message = (
            "TEST-UNEXPECTED-TIMEOUT | %s | application timed out after "
            "%d seconds with no output"
        ) % (self.lastTestSeen, int(timeout))
        self.message_logger.dump_buffered()
        self.message_logger.buffering = False
        self.log.info(error_message)
        self.log.error("Force-terminating active process(es).")

        browser_pid = browser_pid or proc.pid
        child_pids = self.extract_child_pids(processLog, browser_pid)
        self.log.info("Found child pids: %s" % child_pids)

        if HAVE_PSUTIL:
            try:
                browser_proc = [psutil.Process(browser_pid)]
            except Exception:
                self.log.info("Failed to get proc for pid %d" % browser_pid)
                browser_proc = []
            try:
                child_procs = [psutil.Process(pid) for pid in child_pids]
            except Exception:
                self.log.info("Failed to get child procs")
                child_procs = []
            for pid in child_pids:
                self.killAndGetStack(
                    pid, utilityPath, debuggerInfo, dump_screen=not debuggerInfo
                )
            gone, alive = psutil.wait_procs(child_procs, timeout=30)
            for p in gone:
                self.log.info("psutil found pid %s dead" % p.pid)
            for p in alive:
                self.log.warning("failed to kill pid %d after 30s" % p.pid)
            self.killAndGetStack(
                browser_pid, utilityPath, debuggerInfo, dump_screen=not debuggerInfo
            )
            gone, alive = psutil.wait_procs(browser_proc, timeout=30)
            for p in gone:
                self.log.info("psutil found pid %s dead" % p.pid)
            for p in alive:
                self.log.warning("failed to kill pid %d after 30s" % p.pid)
        else:
            self.log.error(
                "psutil not available! Will wait 30s before "
                "attempting to kill parent process. This should "
                "not occur in mozilla automation. See bug 1143547."
            )
            for pid in child_pids:
                self.killAndGetStack(
                    pid, utilityPath, debuggerInfo, dump_screen=not debuggerInfo
                )
            if child_pids:
                time.sleep(30)

            self.killAndGetStack(
                browser_pid, utilityPath, debuggerInfo, dump_screen=not debuggerInfo
            )

    def archiveMozLogs(self):
        if self.mozLogs:
            with zipfile.ZipFile(
                "{}/mozLogs.zip".format(os.environ["MOZ_UPLOAD_DIR"]),
                "w",
                zipfile.ZIP_DEFLATED,
            ) as logzip:
                for logfile in glob.glob(
                    "{}/moz*.log*".format(os.environ["MOZ_UPLOAD_DIR"])
                ):
                    logzip.write(logfile, os.path.basename(logfile))
                    os.remove(logfile)
                logzip.close()

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
            bisectChunk=None,
        ):
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
            handlers = [
                self.fix_stack,
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
                self.harness.countfail += self.shutdownLeaks.process()

            if self.lsanLeaks:
                self.harness.countfail += self.lsanLeaks.process()

        # output message handlers:
        # these take a message and return a message

        def record_result(self, message):
            # by default make the result key equal to pass.
            if message["action"] == "test_start":
                key = message["test"].split("/")[-1].strip()
                self.harness.result[key] = "PASS"
            elif message["action"] == "test_status":
                if "expected" in message:
                    key = message["test"].split("/")[-1].strip()
                    self.harness.result[key] = "FAIL"
                elif message["status"] == "FAIL":
                    key = message["test"].split("/")[-1].strip()
                    self.harness.result[key] = "TODO"
            return message

        def first_error(self, message):
            if (
                message["action"] == "test_status"
                and "expected" in message
                and message["status"] == "FAIL"
            ):
                key = message["test"].split("/")[-1].strip()
                if key not in self.harness.expectedError:
                    self.harness.expectedError[key] = message.get(
                        "message", message["subtest"]
                    ).strip()
            return message

        def countline(self, message):
            if message["action"] == "log":
                line = message.get("message", "")
            elif message["action"] == "process_output":
                line = message.get("data", "")
            else:
                return message
            val = 0
            try:
                val = int(line.split(":")[-1].strip())
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
            if self.stackFixerFunction:
                if message["action"] == "log":
                    message["message"] = self.stackFixerFunction(message["message"])
                elif message["action"] == "process_output":
                    message["data"] = self.stackFixerFunction(message["data"])
            return message

        def record_last_test(self, message):
            """record last test on harness"""
            if message["action"] == "test_start":
                self.harness.lastTestSeen = message["test"]
            elif message["action"] == "test_end":
                if (
                    self.harness.currentTests
                    and message["test"] == self.harness.currentTests[-1]
                ):
                    self.harness.lastTestSeen = "Last test finished"
                else:
                    self.harness.lastTestSeen = "{} (finished)".format(message["test"])
            return message

        def dumpScreenOnTimeout(self, message):
            if (
                not self.dump_screen_on_fail
                and self.dump_screen_on_timeout
                and message["action"] == "test_status"
                and "expected" in message
                and "Test timed out" in message["subtest"]
            ):
                self.harness.dumpScreen(self.utilityPath)
            return message

        def dumpScreenOnFail(self, message):
            if (
                self.dump_screen_on_fail
                and "expected" in message
                and message["status"] == "FAIL"
            ):
                self.harness.dumpScreen(self.utilityPath)
            return message

        def trackLSANLeaks(self, message):
            if self.lsanLeaks and message["action"] in ("log", "process_output"):
                line = (
                    message.get("message", "")
                    if message["action"] == "log"
                    else message["data"]
                )
                self.lsanLeaks.log(line)
            return message

        def trackShutdownLeaks(self, message):
            if self.shutdownLeaks:
                self.shutdownLeaks.log(message)
            return message


def view_gecko_profile_from_mochitest(profile_path, options, profiler_logger):
    """Getting shutdown performance profiles from just the command line arguments is
    difficult. This function makes the developer ergonomics a bit easier by taking the
    generated Gecko profile, and automatically serving it to profiler.firefox.com. The
    Gecko profile during shutdown is dumped to disk at:

    {objdir}/_tests/testing/mochitest/{profilename}

    This function takes that file, and launches a local webserver, and then points
    a browser to profiler.firefox.com to view it. From there it's easy to publish
    or save the profile.
    """

    if options.profilerSaveOnly:
        # The user did not want this to automatically open, only share the location.
        return

    if not os.path.exists(profile_path):
        profiler_logger.error(
            "No profile was found at the profile path, cannot "
            "launch profiler.firefox.com."
        )
        return

    profiler_logger.info("Loading this profile in the Firefox Profiler")

    view_gecko_profile(profile_path)


def run_test_harness(parser, options):
    parser.validate(options)

    logger_options = {
        key: value
        for key, value in six.iteritems(vars(options))
        if key.startswith("log") or key == "valgrind"
    }

    runner = MochitestDesktop(
        options.flavor, logger_options, options.stagedAddons, quiet=options.quiet
    )

    if hasattr(options, "log"):
        delattr(options, "log")

    options.runByManifest = False
    if options.flavor in ("plain", "browser", "chrome"):
        options.runByManifest = True

    if options.verify or options.verify_fission:
        result = runner.verifyTests(options)
    else:
        result = runner.runTests(options)

    runner.archiveMozLogs()
    runner.message_logger.finish()
    return result


def cli(args=sys.argv[1:]):
    # parse command line options
    parser = MochitestArgumentParser(app="generic")
    options = parser.parse_args(args)
    if options is None:
        # parsing error
        sys.exit(1)

    return run_test_harness(parser, options)


if __name__ == "__main__":
    sys.exit(cli())
