#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division, print_function

import copy
import json
import mozdebug
import os
import pipes
import random
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import traceback
import six

from argparse import Namespace
from collections import defaultdict, deque, namedtuple
from datetime import datetime, timedelta
from distutils import dir_util
from functools import partial
from multiprocessing import cpu_count
from subprocess import Popen, PIPE, STDOUT
from tempfile import mkdtemp, gettempdir
from threading import (
    Timer,
    Thread,
    Event,
    current_thread,
)

try:
    import psutil

    HAVE_PSUTIL = True
except Exception:
    HAVE_PSUTIL = False

from xpcshellcommandline import parser_desktop

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))

try:
    from mozbuild.base import MozbuildObject

    build = MozbuildObject.from_environment(cwd=SCRIPT_DIR)
except ImportError:
    build = None

HARNESS_TIMEOUT = 5 * 60

# benchmarking on tbpl revealed that this works best for now
NUM_THREADS = int(cpu_count() * 4)

EXPECTED_LOG_ACTIONS = set(
    [
        "test_status",
        "log",
    ]
)

# --------------------------------------------------------------
# TODO: this is a hack for mozbase without virtualenv, remove with bug 849900
#
here = os.path.dirname(__file__)
mozbase = os.path.realpath(os.path.join(os.path.dirname(here), "mozbase"))

if os.path.isdir(mozbase):
    for package in os.listdir(mozbase):
        sys.path.append(os.path.join(mozbase, package))

from manifestparser import TestManifest
from manifestparser.filters import chunk_by_slice, tags, pathprefix, failures
from manifestparser.util import normsep
from mozlog import commandline
import mozcrash
import mozfile
import mozinfo
from mozprofile import Profile
from mozprofile.cli import parse_preferences
from mozrunner.utils import get_stack_fixer_function

# --------------------------------------------------------------

# TODO: perhaps this should be in a more generally shared location?
# This regex matches all of the C0 and C1 control characters
# (U+0000 through U+001F; U+007F; U+0080 through U+009F),
# except TAB (U+0009), CR (U+000D), LF (U+000A) and backslash (U+005C).
# A raw string is deliberately not used.
_cleanup_encoding_re = re.compile(u"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x9f\\\\]")


def _cleanup_encoding_repl(m):
    c = m.group(0)
    return "\\\\" if c == "\\" else "\\x{0:02X}".format(ord(c))


def cleanup_encoding(s):
    """S is either a byte or unicode string.  Either way it may
    contain control characters, unpaired surrogates, reserved code
    points, etc.  If it is a byte string, it is assumed to be
    UTF-8, but it may not be *correct* UTF-8.  Return a
    sanitized unicode object."""
    if not isinstance(s, six.string_types):
        if isinstance(s, six.binary_type):
            return six.ensure_str(s)
        else:
            return six.text_type(s)
    if isinstance(s, six.binary_type):
        s = s.decode("utf-8", "replace")
    # Replace all C0 and C1 control characters with \xNN escapes.
    return _cleanup_encoding_re.sub(_cleanup_encoding_repl, s)


""" Control-C handling """
gotSIGINT = False


def markGotSIGINT(signum, stackFrame):
    global gotSIGINT
    gotSIGINT = True


class XPCShellTestThread(Thread):
    def __init__(
        self, test_object, retry=True, verbose=False, usingTSan=False, **kwargs
    ):
        Thread.__init__(self)
        self.daemon = True

        self.test_object = test_object
        self.retry = retry
        self.verbose = verbose
        self.usingTSan = usingTSan

        self.appPath = kwargs.get("appPath")
        self.xrePath = kwargs.get("xrePath")
        self.utility_path = kwargs.get("utility_path")
        self.testingModulesDir = kwargs.get("testingModulesDir")
        self.debuggerInfo = kwargs.get("debuggerInfo")
        self.jsDebuggerInfo = kwargs.get("jsDebuggerInfo")
        self.pluginsPath = kwargs.get("pluginsPath")
        self.httpdJSPath = kwargs.get("httpdJSPath")
        self.headJSPath = kwargs.get("headJSPath")
        self.testharnessdir = kwargs.get("testharnessdir")
        self.profileName = kwargs.get("profileName")
        self.singleFile = kwargs.get("singleFile")
        self.env = copy.deepcopy(kwargs.get("env"))
        self.symbolsPath = kwargs.get("symbolsPath")
        self.logfiles = kwargs.get("logfiles")
        self.xpcshell = kwargs.get("xpcshell")
        self.xpcsRunArgs = kwargs.get("xpcsRunArgs")
        self.failureManifest = kwargs.get("failureManifest")
        self.jscovdir = kwargs.get("jscovdir")
        self.stack_fixer_function = kwargs.get("stack_fixer_function")
        self._rootTempDir = kwargs.get("tempDir")
        self.cleanup_dir_list = kwargs.get("cleanup_dir_list")
        self.pStdout = kwargs.get("pStdout")
        self.pStderr = kwargs.get("pStderr")
        self.keep_going = kwargs.get("keep_going")
        self.log = kwargs.get("log")
        self.app_dir_key = kwargs.get("app_dir_key")
        self.interactive = kwargs.get("interactive")
        self.rootPrefsFile = kwargs.get("rootPrefsFile")
        self.extraPrefs = kwargs.get("extraPrefs")
        self.verboseIfFails = kwargs.get("verboseIfFails")
        self.headless = kwargs.get("headless")
        self.runFailures = kwargs.get("runFailures")
        self.timeoutAsPass = kwargs.get("timeoutAsPass")
        self.crashAsPass = kwargs.get("crashAsPass")
        if self.runFailures:
            retry = False

        # Default the test prefsFile to the rootPrefsFile.
        self.prefsFile = self.rootPrefsFile

        # only one of these will be set to 1. adding them to the totals in
        # the harness
        self.passCount = 0
        self.todoCount = 0
        self.failCount = 0

        # Context for output processing
        self.output_lines = []
        self.has_failure_output = False
        self.saw_proc_start = False
        self.saw_proc_end = False
        self.command = None
        self.harness_timeout = kwargs.get("harness_timeout")
        self.timedout = False

        # event from main thread to signal work done
        self.event = kwargs.get("event")
        self.done = False  # explicitly set flag so we don't rely on thread.isAlive

    def run(self):
        try:
            self.run_test()
        except Exception as e:
            self.exception = e
            self.traceback = traceback.format_exc()
        else:
            self.exception = None
            self.traceback = None
        if self.retry:
            self.log.info(
                "%s failed or timed out, will retry." % self.test_object["id"]
            )
        self.done = True
        self.event.set()

    def kill(self, proc):
        """
        Simple wrapper to kill a process.
        On a remote system, this is overloaded to handle remote process communication.
        """
        return proc.kill()

    def removeDir(self, dirname):
        """
        Simple wrapper to remove (recursively) a given directory.
        On a remote system, we need to overload this to work on the remote filesystem.
        """
        mozfile.remove(dirname)

    def poll(self, proc):
        """
        Simple wrapper to check if a process has terminated.
        On a remote system, this is overloaded to handle remote process communication.
        """
        return proc.poll()

    def createLogFile(self, test_file, stdout):
        """
        For a given test file and stdout buffer, create a log file.
        On a remote system we have to fix the test name since it can contain directories.
        """
        with open(test_file + ".log", "w") as f:
            f.write(stdout)

    def getReturnCode(self, proc):
        """
        Simple wrapper to get the return code for a given process.
        On a remote system we overload this to work with the remote process management.
        """
        if proc is not None and hasattr(proc, "returncode"):
            return proc.returncode
        return -1

    def communicate(self, proc):
        """
        Simple wrapper to communicate with a process.
        On a remote system, this is overloaded to handle remote process communication.
        """
        # Processing of incremental output put here to
        # sidestep issues on remote platforms, where what we know
        # as proc is a file pulled off of a device.
        if proc.stdout:
            while True:
                line = proc.stdout.readline()
                if not line:
                    break
                self.process_line(line)

            if self.saw_proc_start and not self.saw_proc_end:
                self.has_failure_output = True

        return proc.communicate()

    def launchProcess(self, cmd, stdout, stderr, env, cwd, timeout=None):
        """
        Simple wrapper to launch a process.
        On a remote system, this is more complex and we need to overload this function.
        """
        # timeout is needed by remote xpcshell to extend the
        # remote device timeout. It is not used in this function.
        if six.PY3:
            cwd = six.ensure_str(cwd)
            for i in range(len(cmd)):
                cmd[i] = six.ensure_str(cmd[i])

        if HAVE_PSUTIL:
            popen_func = psutil.Popen
        else:
            popen_func = Popen

        cleanup_hack = None
        if mozinfo.isWin and sys.version_info[0] == 3 and sys.version_info < (3, 7, 5):
            # Hack to work around https://bugs.python.org/issue37380
            cleanup_hack = subprocess._cleanup

        try:
            if cleanup_hack:
                subprocess._cleanup = lambda: None
            proc = popen_func(cmd, stdout=stdout, stderr=stderr, env=env, cwd=cwd)
        finally:
            if cleanup_hack:
                subprocess._cleanup = cleanup_hack
        return proc

    def checkForCrashes(self, dump_directory, symbols_path, test_name=None):
        """
        Simple wrapper to check for crashes.
        On a remote system, this is more complex and we need to overload this function.
        """
        quiet = False
        if self.crashAsPass:
            quiet = True

        return mozcrash.log_crashes(
            self.log, dump_directory, symbols_path, test=test_name, quiet=quiet
        )

    def logCommand(self, name, completeCmd, testdir):
        self.log.info("%s | full command: %r" % (name, completeCmd))
        self.log.info("%s | current directory: %r" % (name, testdir))
        # Show only those environment variables that are changed from
        # the ambient environment.
        changedEnv = set("%s=%s" % i for i in six.iteritems(self.env)) - set(
            "%s=%s" % i for i in six.iteritems(os.environ)
        )
        self.log.info("%s | environment: %s" % (name, list(changedEnv)))
        shell_command_tokens = [
            pipes.quote(tok) for tok in list(changedEnv) + completeCmd
        ]
        self.log.info(
            "%s | as shell command: (cd %s; %s)"
            % (name, pipes.quote(testdir), " ".join(shell_command_tokens))
        )

    def killTimeout(self, proc):
        if proc is not None and hasattr(proc, "pid"):
            mozcrash.kill_and_get_minidump(
                proc.pid, self.tempDir, utility_path=self.utility_path
            )
        else:
            self.log.info("not killing -- proc or pid unknown")

    def postCheck(self, proc):
        """Checks for a still-running test process, kills it and fails the test if found.
        We can sometimes get here before the process has terminated, which would
        cause removeDir() to fail - so check for the process and kill it if needed.
        """
        if proc and self.poll(proc) is None:
            if HAVE_PSUTIL:
                try:
                    self.kill(proc)
                except psutil.NoSuchProcess:
                    pass
            else:
                self.kill(proc)
            message = "%s | Process still running after test!" % self.test_object["id"]
            if self.retry:
                self.log.info(message)
                return

            self.log.error(message)
            self.log_full_output()
            self.failCount = 1

    def testTimeout(self, proc):
        if self.test_object["expected"] == "pass":
            expected = "PASS"
        else:
            expected = "FAIL"

        if self.retry:
            self.log.test_end(
                self.test_object["id"],
                "TIMEOUT",
                expected="TIMEOUT",
                message="Test timed out",
            )
        else:
            result = "TIMEOUT"
            if self.timeoutAsPass:
                expected = "FAIL"
                result = "FAIL"
            self.failCount = 1
            self.log.test_end(
                self.test_object["id"],
                result,
                expected=expected,
                message="Test timed out",
            )
            self.log_full_output()

        self.done = True
        self.timedout = True
        self.killTimeout(proc)
        self.log.info("xpcshell return code: %s" % self.getReturnCode(proc))
        self.postCheck(proc)
        self.clean_temp_dirs(self.test_object["path"])

    def updateTestPrefsFile(self):
        # If the Manifest file has some additiona prefs, merge the
        # prefs set in the user.js file stored in the _rootTempdir
        # with the prefs from the manifest and the prefs specified
        # in the extraPrefs option.
        if "prefs" in self.test_object:
            # Merge the user preferences in a fake profile dir in a
            # local temporary dir (self.tempDir is the remoteTmpDir
            # for the RemoteXPCShellTestThread subclass and so we
            # can't use that tempDir here).
            localTempDir = mkdtemp(prefix="xpc-other-", dir=self._rootTempDir)

            filename = "user.js"
            interpolation = {"server": "dummyserver"}
            profile = Profile(profile=localTempDir, restore=False)
            profile.merge(self._rootTempDir, interpolation=interpolation)

            prefs = self.test_object["prefs"].strip().split()
            name = self.test_object["id"]
            if self.verbose:
                self.log.info(
                    "%s: Per-test extra prefs will be set:\n  {}".format(
                        "\n  ".join(prefs)
                    )
                    % name
                )

            profile.set_preferences(parse_preferences(prefs), filename=filename)
            # Make sure that the extra prefs form the command line are overriding
            # any prefs inherited from the shared profile data or the manifest prefs.
            profile.set_preferences(
                parse_preferences(self.extraPrefs), filename=filename
            )
            return os.path.join(profile.profile, filename)

        # Return the root prefsFile if there is no other prefs to merge.
        return self.rootPrefsFile

    def buildCmdTestFile(self, name):
        """
        Build the command line arguments for the test file.
        On a remote system, this may be overloaded to use a remote path structure.
        """
        return ["-e", 'const _TEST_FILE = ["%s"];' % name.replace("\\", "/")]

    def setupTempDir(self):
        tempDir = mkdtemp(prefix="xpc-other-", dir=self._rootTempDir)
        self.env["XPCSHELL_TEST_TEMP_DIR"] = tempDir
        if self.interactive:
            self.log.info("temp dir is %s" % tempDir)
        return tempDir

    def setupPluginsDir(self):
        if not os.path.isdir(self.pluginsPath):
            return None

        pluginsDir = mkdtemp(prefix="xpc-plugins-", dir=self._rootTempDir)
        retries = 0
        while not os.path.isdir(pluginsDir) and retries < 5:
            self.log.info("plugins temp directory %s missing; waiting..." % pluginsDir)
            time.sleep(1)
            retries += 1
        # shutil.copytree requires dst to not exist. Deleting the tempdir
        # would make a race condition possible in a concurrent environment,
        # so we are using dir_utils.copy_tree which accepts an existing dst
        dir_util.copy_tree(self.pluginsPath, pluginsDir)
        if self.interactive:
            self.log.info("plugins dir is %s" % pluginsDir)
        return pluginsDir

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
            except Exception:
                pass
            os.makedirs(profileDir)
        else:
            profileDir = mkdtemp(prefix="xpc-profile-", dir=self._rootTempDir)
        self.env["XPCSHELL_TEST_PROFILE_DIR"] = profileDir
        if self.interactive or self.singleFile:
            self.log.info("profile dir is %s" % profileDir)
        return profileDir

    def setupMozinfoJS(self):
        mozInfoJSPath = os.path.join(self.profileDir, "mozinfo.json")
        mozInfoJSPath = mozInfoJSPath.replace("\\", "\\\\")
        mozinfo.output_to_file(mozInfoJSPath)
        return mozInfoJSPath

    def buildCmdHead(self):
        """
        Build the command line arguments for the head files,
        along with the address of the webserver which some tests require.

        On a remote system, this is overloaded to resolve quoting issues over a
        secondary command line.
        """
        headfiles = self.getHeadFiles(self.test_object)
        cmdH = ", ".join(['"' + f.replace("\\", "/") + '"' for f in headfiles])

        dbgport = 0 if self.jsDebuggerInfo is None else self.jsDebuggerInfo.port

        return [
            "-e",
            "const _HEAD_FILES = [%s];" % cmdH,
            "-e",
            "const _JSDEBUGGER_PORT = %d;" % dbgport,
        ]

    def getHeadFiles(self, test):
        """Obtain lists of head- files.  Returns a list of head files."""

        def sanitize_list(s, kind):
            for f in s.strip().split(" "):
                f = f.strip()
                if len(f) < 1:
                    continue

                path = os.path.normpath(os.path.join(test["here"], f))
                if not os.path.exists(path):
                    raise Exception("%s file does not exist: %s" % (kind, path))

                if not os.path.isfile(path):
                    raise Exception("%s file is not a file: %s" % (kind, path))

                yield path

        headlist = test.get("head", "")
        return list(sanitize_list(headlist, "head"))

    def buildXpcsCmd(self):
        """
        Load the root head.js file as the first file in our test path, before other head,
        and test files. On a remote system, we overload this to add additional command
        line arguments, so this gets overloaded.
        """
        # - NOTE: if you rename/add any of the constants set here, update
        #   do_load_child_test_harness() in head.js
        if not self.appPath:
            self.appPath = self.xrePath

        xpcsCmd = [
            self.xpcshell,
            "-g",
            self.xrePath,
            "-a",
            self.appPath,
            "-m",
            "-e",
            'const _HEAD_JS_PATH = "%s";' % self.headJSPath,
            "-e",
            'const _MOZINFO_JS_PATH = "%s";' % self.mozInfoJSPath,
            "-e",
            'const _PREFS_FILE = "%s";' % self.prefsFile.replace("\\", "\\\\"),
        ]

        if self.testingModulesDir:
            # Escape backslashes in string literal.
            sanitized = self.testingModulesDir.replace("\\", "\\\\")
            xpcsCmd.extend(["-e", 'const _TESTING_MODULES_DIR = "%s";' % sanitized])

        xpcsCmd.extend(["-f", os.path.join(self.testharnessdir, "head.js")])

        if self.debuggerInfo:
            xpcsCmd = [self.debuggerInfo.path] + self.debuggerInfo.args + xpcsCmd

        # Automation doesn't specify a pluginsPath and xpcshell defaults to
        # $APPDIR/plugins. We do the same here so we can carry on with
        # setting up every test with its own plugins directory.
        if not self.pluginsPath:
            self.pluginsPath = os.path.join(self.appPath, "plugins")

        self.pluginsDir = self.setupPluginsDir()
        if self.pluginsDir:
            xpcsCmd.extend(["-p", self.pluginsDir])

        return xpcsCmd

    def cleanupDir(self, directory, name):
        if not os.path.exists(directory):
            return

        # up to TRY_LIMIT attempts (one every second), because
        # the Windows filesystem is slow to react to the changes
        TRY_LIMIT = 25
        try_count = 0
        while try_count < TRY_LIMIT:
            try:
                self.removeDir(directory)
            except OSError:
                self.log.info("Failed to remove directory: %s. Waiting." % directory)
                # We suspect the filesystem may still be making changes. Wait a
                # little bit and try again.
                time.sleep(1)
                try_count += 1
            else:
                # removed fine
                return

        # we try cleaning up again later at the end of the run
        self.cleanup_dir_list.append(directory)

    def clean_temp_dirs(self, name):
        # We don't want to delete the profile when running check-interactive
        # or check-one.
        if self.profileDir and not self.interactive and not self.singleFile:
            self.cleanupDir(self.profileDir, name)

        self.cleanupDir(self.tempDir, name)

        if self.pluginsDir:
            self.cleanupDir(self.pluginsDir, name)

    def parse_output(self, output):
        """Parses process output for structured messages and saves output as it is
        read. Sets self.has_failure_output in case of evidence of a failure"""
        for line_string in output.splitlines():
            self.process_line(line_string)

        if self.saw_proc_start and not self.saw_proc_end:
            self.has_failure_output = True

    def fix_text_output(self, line):
        line = cleanup_encoding(line)
        if self.stack_fixer_function is not None:
            line = self.stack_fixer_function(line)

        if isinstance(line, bytes):
            line = line.decode("utf-8")
        return line

    def log_line(self, line):
        """Log a line of output (either a parser json object or text output from
        the test process"""
        if isinstance(line, six.string_types) or isinstance(line, bytes):
            line = self.fix_text_output(line).rstrip("\r\n")
            self.log.process_output(self.proc_ident, line, command=self.command)
        else:
            if "message" in line:
                line["message"] = self.fix_text_output(line["message"])
            if "xpcshell_process" in line:
                line["thread"] = " ".join(
                    [current_thread().name, line["xpcshell_process"]]
                )
            else:
                line["thread"] = current_thread().name
            self.log.log_raw(line)

    def log_full_output(self):
        """Logs any buffered output from the test process, and clears the buffer."""
        if not self.output_lines:
            return
        self.log.info(">>>>>>>")
        for line in self.output_lines:
            self.log_line(line)
        self.log.info("<<<<<<<")
        self.output_lines = []

    def report_message(self, message):
        """Stores or logs a json log message in mozlog format."""
        if self.verbose:
            self.log_line(message)
        else:
            self.output_lines.append(message)

    def process_line(self, line_string):
        """Parses a single line of output, determining its significance and
        reporting a message.
        """
        if not line_string.strip():
            return

        try:
            line_object = json.loads(line_string)
            if not isinstance(line_object, dict):
                self.report_message(line_string)
                return
        except ValueError:
            self.report_message(line_string)
            return

        if (
            "action" not in line_object
            or line_object["action"] not in EXPECTED_LOG_ACTIONS
        ):
            # The test process output JSON.
            self.report_message(line_string)
            return

        action = line_object["action"]

        self.has_failure_output = (
            self.has_failure_output
            or "expected" in line_object
            or action == "log"
            and line_object["level"] == "ERROR"
        )

        self.report_message(line_object)

        if action == "log" and line_object["message"] == "CHILD-TEST-STARTED":
            self.saw_proc_start = True
        elif action == "log" and line_object["message"] == "CHILD-TEST-COMPLETED":
            self.saw_proc_end = True

    def run_test(self):
        """Run an individual xpcshell test."""
        global gotSIGINT

        name = self.test_object["id"]
        path = self.test_object["path"]

        # Check for skipped tests
        if "disabled" in self.test_object:
            message = self.test_object["disabled"]
            if not message:
                message = "disabled from xpcshell manifest"
            self.log.test_start(name)
            self.log.test_end(name, "SKIP", message=message)

            self.retry = False
            self.keep_going = True
            return

        # Check for known-fail tests
        expect_pass = self.test_object["expected"] == "pass"

        # By default self.appPath will equal the gre dir. If specified in the
        # xpcshell.ini file, set a different app dir for this test.
        if self.app_dir_key and self.app_dir_key in self.test_object:
            rel_app_dir = self.test_object[self.app_dir_key]
            rel_app_dir = os.path.join(self.xrePath, rel_app_dir)
            self.appPath = os.path.abspath(rel_app_dir)
        else:
            self.appPath = None

        test_dir = os.path.dirname(path)

        # Create a profile and a temp dir that the JS harness can stick
        # a profile and temporary data in
        self.profileDir = self.setupProfileDir()
        self.tempDir = self.setupTempDir()
        self.mozInfoJSPath = self.setupMozinfoJS()

        # Setup per-manifest prefs and write them into the tempdir.
        self.prefsFile = self.updateTestPrefsFile()

        # The order of the command line is important:
        # 1) Arguments for xpcshell itself
        self.command = self.buildXpcsCmd()

        # 2) Arguments for the head files
        self.command.extend(self.buildCmdHead())

        # 3) Arguments for the test file
        self.command.extend(self.buildCmdTestFile(path))
        self.command.extend(["-e", 'const _TEST_NAME = "%s";' % name])

        # 4) Arguments for code coverage
        if self.jscovdir:
            self.command.extend(
                ["-e", 'const _JSCOV_DIR = "%s";' % self.jscovdir.replace("\\", "/")]
            )

        # 5) Runtime arguments
        if "debug" in self.test_object:
            self.command.append("-d")

        self.command.extend(self.xpcsRunArgs)

        if self.test_object.get("dmd") == "true":
            self.env["PYTHON"] = sys.executable
            self.env["BREAKPAD_SYMBOLS_PATH"] = self.symbolsPath

        if self.test_object.get("subprocess") == "true":
            self.env["PYTHON"] = sys.executable

        if (
            self.test_object.get("headless", "true" if self.headless else None)
            == "true"
        ):
            self.env["MOZ_HEADLESS"] = "1"
            self.env["DISPLAY"] = "77"  # Set a fake display.

        testTimeoutInterval = self.harness_timeout
        # Allow a test to request a multiple of the timeout if it is expected to take long
        if "requesttimeoutfactor" in self.test_object:
            testTimeoutInterval *= int(self.test_object["requesttimeoutfactor"])

        testTimer = None
        if not self.interactive and not self.debuggerInfo and not self.jsDebuggerInfo:
            testTimer = Timer(testTimeoutInterval, lambda: self.testTimeout(proc))
            testTimer.start()

        proc = None
        process_output = None

        try:
            self.log.test_start(name)
            if self.verbose:
                self.logCommand(name, self.command, test_dir)

            proc = self.launchProcess(
                self.command,
                stdout=self.pStdout,
                stderr=self.pStderr,
                env=self.env,
                cwd=test_dir,
                timeout=testTimeoutInterval,
            )

            if hasattr(proc, "pid"):
                self.proc_ident = proc.pid
            else:
                # On mobile, "proc" is just a file.
                self.proc_ident = name

            if self.interactive:
                self.log.info("%s | Process ID: %d" % (name, self.proc_ident))

            # Communicate returns a tuple of (stdout, stderr), however we always
            # redirect stderr to stdout, so the second element is ignored.
            process_output, _ = self.communicate(proc)

            if self.interactive:
                # Not sure what else to do here...
                self.keep_going = True
                return

            if testTimer:
                testTimer.cancel()

            if process_output:
                # For the remote case, stdout is not yet depleted, so we parse
                # it here all at once.
                self.parse_output(process_output)

            return_code = self.getReturnCode(proc)

            # TSan'd processes return 66 if races are detected.  This isn't
            # good in the sense that there's no way to distinguish between
            # a process that would normally have returned zero but has races,
            # and a race-free process that returns 66.  But I don't see how
            # to do better.  This ambiguity is at least constrained to the
            # with-TSan case.  It doesn't affect normal builds.
            #
            # This also assumes that the magic value 66 isn't overridden by
            # a TSAN_OPTIONS=exitcode=<number> environment variable setting.
            #
            TSAN_EXIT_CODE_WITH_RACES = 66

            return_code_ok = return_code == 0 or (
                self.usingTSan and return_code == TSAN_EXIT_CODE_WITH_RACES
            )
            passed = (not self.has_failure_output) and return_code_ok

            status = "PASS" if passed else "FAIL"
            expected = "PASS" if expect_pass else "FAIL"
            message = "xpcshell return code: %d" % return_code

            if self.timedout:
                return

            if status != expected:
                if self.retry:
                    self.log.test_end(
                        name,
                        status,
                        expected=status,
                        message="Test failed or timed out, will retry",
                    )
                    self.clean_temp_dirs(path)
                    if self.verboseIfFails and not self.verbose:
                        self.log_full_output()
                    return

                self.log.test_end(name, status, expected=expected, message=message)
                self.log_full_output()

                self.failCount += 1

                if self.failureManifest:
                    with open(self.failureManifest, "a") as f:
                        f.write("[%s]\n" % self.test_object["path"])
                        for k, v in self.test_object.items():
                            f.write("%s = %s\n" % (k, v))

            else:
                # If TSan reports a race, dump the output, else we can't
                # diagnose what the problem was.  See comments above about
                # the significance of TSAN_EXIT_CODE_WITH_RACES.
                if self.usingTSan and return_code == TSAN_EXIT_CODE_WITH_RACES:
                    self.log_full_output()

                self.log.test_end(name, status, expected=expected, message=message)
                if self.verbose:
                    self.log_full_output()

                self.retry = False

                if expect_pass:
                    self.passCount = 1
                else:
                    self.todoCount = 1

            if self.checkForCrashes(self.tempDir, self.symbolsPath, test_name=name):
                if self.retry:
                    self.clean_temp_dirs(path)
                    return

                # If we assert during shutdown there's a chance the test has passed
                # but we haven't logged full output, so do so here.
                self.log_full_output()
                self.failCount = 1

            if self.logfiles and process_output:
                self.createLogFile(name, process_output)

        finally:
            self.postCheck(proc)
            self.clean_temp_dirs(path)

        if gotSIGINT:
            self.log.error("Received SIGINT (control-C) during test execution")
            if self.keep_going:
                gotSIGINT = False
            else:
                self.keep_going = False
                return

        self.keep_going = True


class XPCShellTests(object):
    def __init__(self, log=None):
        """Initializes node status and logger."""
        self.log = log
        self.harness_timeout = HARNESS_TIMEOUT
        self.nodeProc = {}
        self.http3ServerProc = {}

    def getTestManifest(self, manifest):
        if isinstance(manifest, TestManifest):
            return manifest
        elif manifest is not None:
            manifest = os.path.normpath(os.path.abspath(manifest))
            if os.path.isfile(manifest):
                return TestManifest([manifest], strict=True)
            else:
                ini_path = os.path.join(manifest, "xpcshell.ini")
        else:
            ini_path = os.path.join(SCRIPT_DIR, "tests", "xpcshell.ini")

        if os.path.exists(ini_path):
            return TestManifest([ini_path], strict=True)
        else:
            self.log.error(
                "Failed to find manifest at %s; use --manifest "
                "to set path explicitly." % ini_path
            )
            sys.exit(1)

    def normalizeTest(self, root, test_object):
        path = test_object.get("file_relpath", test_object["relpath"])
        if "dupe-manifest" in test_object and "ancestor_manifest" in test_object:
            test_object["id"] = "%s:%s" % (
                os.path.basename(test_object["ancestor_manifest"]),
                path,
            )
        else:
            test_object["id"] = path

        if root:
            test_object["manifest"] = os.path.relpath(test_object["manifest"], root)

        if os.sep != "/":
            for key in ("id", "manifest"):
                test_object[key] = test_object[key].replace(os.sep, "/")

        return test_object

    def buildTestList(self, test_tags=None, test_paths=None, verify=False):
        """Reads the xpcshell.ini manifest and set self.alltests to an array.

        Given the parameters, this method compiles a list of tests to be run
        that matches the criteria set by parameters.

        If any chunking of tests are to occur, it is also done in this method.

        If no tests are added to the list of tests to be run, an error
        is logged. A sys.exit() signal is sent to the caller.

        Args:
            test_tags (list, optional): list of strings.
            test_paths (list, optional): list of strings derived from the command
                                         line argument provided by user, specifying
                                         tests to be run.
            verify (bool, optional): boolean value.
        """
        if test_paths is None:
            test_paths = []

        mp = self.getTestManifest(self.manifest)

        root = mp.rootdir
        if build and not root:
            root = build.topsrcdir
        normalize = partial(self.normalizeTest, root)

        filters = []
        if test_tags:
            filters.append(tags(test_tags))

        path_filter = None
        if test_paths:
            path_filter = pathprefix(test_paths)
            filters.append(path_filter)

        noDefaultFilters = False
        if self.runFailures:
            filters.append(failures(self.runFailures))
            noDefaultFilters = True

        if self.totalChunks > 1:
            filters.append(chunk_by_slice(self.thisChunk, self.totalChunks))
        try:
            self.alltests = list(
                map(
                    normalize,
                    mp.active_tests(
                        filters=filters,
                        noDefaultFilters=noDefaultFilters,
                        **mozinfo.info
                    ),
                )
            )
        except TypeError:
            sys.stderr.write("*** offending mozinfo.info: %s\n" % repr(mozinfo.info))
            raise

        if path_filter and path_filter.missing:
            self.log.warning(
                "The following path(s) didn't resolve any tests:\n  {}".format(
                    "  \n".join(sorted(path_filter.missing))
                )
            )

        if len(self.alltests) == 0:
            if (
                test_paths
                and path_filter.missing == set(test_paths)
                and os.environ.get("MOZ_AUTOMATION") == "1"
            ):
                # This can happen in CI when a manifest doesn't exist due to a
                # build config variable in moz.build traversal. Don't generate
                # an error in this case. Adding a todo count avoids mozharness
                # raising an error.
                self.todoCount += len(path_filter.missing)
            else:
                self.log.error(
                    "no tests to run using specified "
                    "combination of filters: {}".format(mp.fmt_filters())
                )
                sys.exit(1)

        if len(self.alltests) == 1 and not verify:
            self.singleFile = os.path.basename(self.alltests[0]["path"])
        else:
            self.singleFile = None

        if self.dump_tests:
            self.dump_tests = os.path.expanduser(self.dump_tests)
            assert os.path.exists(os.path.dirname(self.dump_tests))
            with open(self.dump_tests, "w") as dumpFile:
                dumpFile.write(json.dumps({"active_tests": self.alltests}))

            self.log.info("Dumping active_tests to %s file." % self.dump_tests)
            sys.exit()

    def setAbsPath(self):
        """
        Set the absolute path for xpcshell, httpdjspath and xrepath. These 3 variables
        depend on input from the command line and we need to allow for absolute paths.
        This function is overloaded for a remote solution as os.path* won't work remotely.
        """
        self.testharnessdir = os.path.dirname(os.path.abspath(__file__))
        self.headJSPath = self.testharnessdir.replace("\\", "/") + "/head.js"
        self.xpcshell = os.path.abspath(self.xpcshell)

        if self.xrePath is None:
            self.xrePath = os.path.dirname(self.xpcshell)
            if mozinfo.isMac:
                # Check if we're run from an OSX app bundle and override
                # self.xrePath if we are.
                appBundlePath = os.path.join(
                    os.path.dirname(os.path.dirname(self.xpcshell)), "Resources"
                )
                if os.path.exists(os.path.join(appBundlePath, "application.ini")):
                    self.xrePath = appBundlePath
        else:
            self.xrePath = os.path.abspath(self.xrePath)

        # httpd.js belongs in xrePath/components, which is Contents/Resources on mac
        self.httpdJSPath = os.path.join(self.xrePath, "components", "httpd.js")
        self.httpdJSPath = self.httpdJSPath.replace("\\", "/")

        if self.mozInfo is None:
            self.mozInfo = os.path.join(self.testharnessdir, "mozinfo.json")

    def buildPrefsFile(self, extraPrefs):
        # Create the prefs.js file

        # In test packages used in CI, the profile_data directory is installed
        # in the SCRIPT_DIR.
        profile_data_dir = os.path.join(SCRIPT_DIR, "profile_data")
        # If possible, read profile data from topsrcdir. This prevents us from
        # requiring a re-build to pick up newly added extensions in the
        # <profile>/extensions directory.
        if build:
            path = os.path.join(build.topsrcdir, "testing", "profiles")
            if os.path.isdir(path):
                profile_data_dir = path
        # Still not found? Look for testing/profiles relative to testing/xpcshell.
        if not os.path.isdir(profile_data_dir):
            path = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "profiles"))
            if os.path.isdir(path):
                profile_data_dir = path

        with open(os.path.join(profile_data_dir, "profiles.json"), "r") as fh:
            base_profiles = json.load(fh)["xpcshell"]

        # values to use when interpolating preferences
        interpolation = {
            "server": "dummyserver",
        }

        profile = Profile(profile=self.tempDir, restore=False)
        prefsFile = os.path.join(profile.profile, "user.js")

        # Empty the user.js file in case the file existed before.
        with open(prefsFile, "w"):
            pass

        for name in base_profiles:
            path = os.path.join(profile_data_dir, name)
            profile.merge(path, interpolation=interpolation)

        # add command line prefs
        prefs = parse_preferences(extraPrefs)
        profile.set_preferences(prefs)

        self.prefsFile = prefsFile
        return prefs

    def buildCoreEnvironment(self):
        """
        Add environment variables likely to be used across all platforms, including
        remote systems.
        """
        # Make assertions fatal
        self.env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
        # Crash reporting interferes with debugging
        if not self.debuggerInfo:
            self.env["MOZ_CRASHREPORTER"] = "1"
        # Don't launch the crash reporter client
        self.env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        # Don't permit remote connections by default.
        # MOZ_DISABLE_NONLOCAL_CONNECTIONS can be set to "0" to temporarily
        # enable non-local connections for the purposes of local testing.
        # Don't override the user's choice here.  See bug 1049688.
        self.env.setdefault("MOZ_DISABLE_NONLOCAL_CONNECTIONS", "1")
        if self.mozInfo.get("topsrcdir") is not None:
            self.env["MOZ_DEVELOPER_REPO_DIR"] = self.mozInfo["topsrcdir"]
        if self.mozInfo.get("topobjdir") is not None:
            self.env["MOZ_DEVELOPER_OBJ_DIR"] = self.mozInfo["topobjdir"]

        # Disable the content process sandbox for the xpcshell tests. They
        # currently attempt to do things like bind() sockets, which is not
        # compatible with the sandbox.
        self.env["MOZ_DISABLE_CONTENT_SANDBOX"] = "1"

        if self.mozInfo.get("socketprocess_networking"):
            self.env["MOZ_FORCE_USE_SOCKET_PROCESS"] = "1"
        else:
            self.env["MOZ_DISABLE_SOCKET_PROCESS"] = "1"

        if self.enable_webrender:
            self.env["MOZ_WEBRENDER"] = "1"
            self.env["MOZ_ACCELERATED"] = "1"
        else:
            self.env["MOZ_WEBRENDER"] = "0"

    def buildEnvironment(self):
        """
        Create and returns a dictionary of self.env to include all the appropriate env
        variables and values. On a remote system, we overload this to set different
        values and are missing things like os.environ and PATH.
        """
        self.env = dict(os.environ)
        self.buildCoreEnvironment()
        if sys.platform == "win32":
            self.env["PATH"] = self.env["PATH"] + ";" + self.xrePath
        elif sys.platform in ("os2emx", "os2knix"):
            os.environ["BEGINLIBPATH"] = self.xrePath + ";" + self.env["BEGINLIBPATH"]
            os.environ["LIBPATHSTRICT"] = "T"
        elif sys.platform == "osx" or sys.platform == "darwin":
            self.env["DYLD_LIBRARY_PATH"] = os.path.join(
                os.path.dirname(self.xrePath), "MacOS"
            )
        else:  # unix or linux?
            if "LD_LIBRARY_PATH" not in self.env or self.env["LD_LIBRARY_PATH"] is None:
                self.env["LD_LIBRARY_PATH"] = self.xrePath
            else:
                self.env["LD_LIBRARY_PATH"] = ":".join(
                    [self.xrePath, self.env["LD_LIBRARY_PATH"]]
                )

        usingASan = "asan" in self.mozInfo and self.mozInfo["asan"]
        usingTSan = "tsan" in self.mozInfo and self.mozInfo["tsan"]
        if usingASan or usingTSan:
            # symbolizer support
            llvmsym = os.path.join(
                self.xrePath, "llvm-symbolizer" + self.mozInfo["bin_suffix"]
            )
            if os.path.isfile(llvmsym):
                if usingASan:
                    self.env["ASAN_SYMBOLIZER_PATH"] = llvmsym
                else:
                    oldTSanOptions = self.env.get("TSAN_OPTIONS", "")
                    self.env["TSAN_OPTIONS"] = "external_symbolizer_path={} {}".format(
                        llvmsym, oldTSanOptions
                    )
                self.log.info("runxpcshelltests.py | using symbolizer at %s" % llvmsym)
            else:
                self.log.error(
                    "TEST-UNEXPECTED-FAIL | runxpcshelltests.py | "
                    "Failed to find symbolizer at %s" % llvmsym
                )

        return self.env

    def getPipes(self):
        """
        Determine the value of the stdout and stderr for the test.
        Return value is a list (pStdout, pStderr).
        """
        if self.interactive:
            pStdout = None
            pStderr = None
        else:
            if self.debuggerInfo and self.debuggerInfo.interactive:
                pStdout = None
                pStderr = None
            else:
                if sys.platform == "os2emx":
                    pStdout = None
                else:
                    pStdout = PIPE
                pStderr = STDOUT
        return pStdout, pStderr

    def verifyDirPath(self, dirname):
        """
        Simple wrapper to get the absolute path for a given directory name.
        On a remote system, we need to overload this to work on the remote filesystem.
        """
        return os.path.abspath(dirname)

    def trySetupNode(self):
        """
        Run node for HTTP/2 tests, if available, and updates mozinfo as appropriate.
        """
        if os.getenv("MOZ_ASSUME_NODE_RUNNING", None):
            self.log.info("Assuming required node servers are already running")
            if not os.getenv("MOZHTTP2_PORT", None):
                self.log.warning(
                    "MOZHTTP2_PORT environment variable not set. "
                    "Tests requiring http/2 will fail."
                )
            return

        # We try to find the node executable in the path given to us by the user in
        # the MOZ_NODE_PATH environment variable
        nodeBin = os.getenv("MOZ_NODE_PATH", None)
        if not nodeBin and build:
            nodeBin = build.substs.get("NODEJS")
        if not nodeBin:
            self.log.warning(
                "MOZ_NODE_PATH environment variable not set. "
                "Tests requiring http/2 will fail."
            )
            return

        if not os.path.exists(nodeBin) or not os.path.isfile(nodeBin):
            error = "node not found at MOZ_NODE_PATH %s" % (nodeBin)
            self.log.error(error)
            raise IOError(error)

        self.log.info("Found node at %s" % (nodeBin,))

        def startServer(name, serverJs):
            if not os.path.exists(serverJs):
                error = "%s not found at %s" % (name, serverJs)
                self.log.error(error)
                raise IOError(error)

            # OK, we found our server, let's try to get it running
            self.log.info("Found %s at %s" % (name, serverJs))
            try:
                # We pipe stdin to node because the server will exit when its
                # stdin reaches EOF
                process = Popen(
                    [nodeBin, serverJs],
                    stdin=PIPE,
                    stdout=PIPE,
                    stderr=PIPE,
                    env=self.env,
                    cwd=os.getcwd(),
                    universal_newlines=True,
                )
                self.nodeProc[name] = process

                # Check to make sure the server starts properly by waiting for it to
                # tell us it's started
                msg = process.stdout.readline()
                if "server listening" in msg:
                    searchObj = re.search(
                        r"HTTP2 server listening on ports ([0-9]+),([0-9]+)", msg, 0
                    )
                    if searchObj:
                        self.env["MOZHTTP2_PORT"] = searchObj.group(1)
                        self.env["MOZNODE_EXEC_PORT"] = searchObj.group(2)
            except OSError as e:
                # This occurs if the subprocess couldn't be started
                self.log.error("Could not run %s server: %s" % (name, str(e)))
                raise

        myDir = os.path.split(os.path.abspath(__file__))[0]
        startServer("moz-http2", os.path.join(myDir, "moz-http2", "moz-http2.js"))

    def shutdownNode(self):
        """
        Shut down our node process, if it exists
        """
        for name, proc in six.iteritems(self.nodeProc):
            self.log.info("Node %s server shutting down ..." % name)
            if proc.poll() is not None:
                self.log.info("Node server %s already dead %s" % (name, proc.poll()))
            else:
                proc.terminate()

            def dumpOutput(fd, label):
                firstTime = True
                for msg in fd:
                    if firstTime:
                        firstTime = False
                        self.log.info("Process %s" % label)
                    self.log.info(msg)

            dumpOutput(proc.stdout, "stdout")
            dumpOutput(proc.stderr, "stderr")
        self.nodeProc = {}

    def startHttp3Server(self):
        """
        Start a Http3 test server.
        """
        binSuffix = ""
        if sys.platform == "win32":
            binSuffix = ".exe"

        http3ServerPath = self.http3server
        if not http3ServerPath:
            http3ServerPath = os.path.join(
                SCRIPT_DIR, "http3server", "http3server" + binSuffix
            )
            if build:
                http3ServerPath = os.path.join(
                    build.topobjdir, "dist", "bin", "http3server" + binSuffix
                )

        if not os.path.exists(http3ServerPath):
            self.log.warning(
                "Http3 server not found at "
                + http3ServerPath
                + ". Tests requiring http/3 will fail."
            )
            return

        # OK, we found our server, let's try to get it running
        self.log.info("Found %s" % (http3ServerPath))
        try:
            dbPath = os.path.join(SCRIPT_DIR, "http3server", "http3serverDB")
            if build:
                dbPath = os.path.join(
                    build.topsrcdir, "netwerk", "test", "http3serverDB"
                )
            self.log.info("Using %s" % (dbPath))
            # We pipe stdin to the server because it will exit when its stdin
            # reaches EOF
            process = Popen(
                [http3ServerPath, dbPath],
                stdin=PIPE,
                stdout=PIPE,
                stderr=PIPE,
                env=self.env,
                cwd=os.getcwd(),
                universal_newlines=True,
            )
            self.http3ServerProc["http3Server"] = process

            # Check to make sure the server starts properly by waiting for it to
            # tell us it's started
            msg = process.stdout.readline()
            if "server listening" in msg:
                searchObj = re.search(
                    r"HTTP3 server listening on ports ([0-9]+), ([0-9]+), ([0-9]+) and ([0-9]+)."
                    " EchConfig is @([\x00-\x7F]+)@",
                    msg,
                    0,
                )
                if searchObj:
                    self.env["MOZHTTP3_PORT"] = searchObj.group(1)
                    self.env["MOZHTTP3_PORT_FAILED"] = searchObj.group(2)
                    self.env["MOZHTTP3_PORT_ECH"] = searchObj.group(3)
                    self.env["MOZHTTP3_PORT_NO_RESPONSE"] = searchObj.group(4)
                    self.env["MOZHTTP3_ECH"] = searchObj.group(5)
        except OSError as e:
            # This occurs if the subprocess couldn't be started
            self.log.error("Could not run the http3 server: %s" % (str(e)))

    def shutdownHttp3Server(self):
        """
        Shutdown our http3Server process, if it exists
        """
        for name, proc in six.iteritems(self.http3ServerProc):
            self.log.info("%s server shutting down ..." % name)
            if proc.poll() is not None:
                self.log.info("Http3 server %s already dead %s" % (name, proc.poll()))
            else:
                proc.terminate()
                retries = 0
                while proc.poll() is None:
                    time.sleep(0.1)
                    retries += 1
                    if retries > 40:
                        self.log.info("Killing proc")
                        proc.kill()
                        break

            def dumpOutput(fd, label):
                firstTime = True
                for msg in fd:
                    if firstTime:
                        firstTime = False
                        self.log.info("Process %s" % label)
                    self.log.info(msg)

            dumpOutput(proc.stdout, "stdout")
            dumpOutput(proc.stderr, "stderr")
        self.http3ServerProc = {}

    def buildXpcsRunArgs(self):
        """
        Add arguments to run the test or make it interactive.
        """
        if self.interactive:
            self.xpcsRunArgs = [
                "-e",
                'print("To start the test, type |_execute_test();|.");',
                "-i",
            ]
        else:
            self.xpcsRunArgs = ["-e", "_execute_test(); quit(0);"]

    def addTestResults(self, test):
        self.passCount += test.passCount
        self.failCount += test.failCount
        self.todoCount += test.todoCount

    def updateMozinfo(self, prefs, options):
        # Handle filenames in mozInfo
        if not isinstance(self.mozInfo, dict):
            mozInfoFile = self.mozInfo
            if not os.path.isfile(mozInfoFile):
                self.log.error(
                    "Error: couldn't find mozinfo.json at '%s'. Perhaps you "
                    "need to use --build-info-json?" % mozInfoFile
                )
                return False
            self.mozInfo = json.load(open(mozInfoFile))

        # mozinfo.info is used as kwargs.  Some builds are done with
        # an older Python that can't handle Unicode keys in kwargs.
        # All of the keys in question should be ASCII.
        fixedInfo = {}
        for k, v in self.mozInfo.items():
            if isinstance(k, bytes):
                k = k.decode("utf-8")
            fixedInfo[k] = v
        self.mozInfo = fixedInfo

        self.mozInfo["fission"] = prefs.get("fission.autostart", False)

        self.mozInfo["serviceworker_e10s"] = True

        self.mozInfo["verify"] = options.get("verify", False)
        self.mozInfo["webrender"] = self.enable_webrender

        self.mozInfo["socketprocess_networking"] = prefs.get(
            "network.http.network_access_on_socket_process.enabled", False
        )

        mozinfo.update(self.mozInfo)

        return True

    def runSelfTest(self):
        import selftest
        import unittest

        this = self

        class XPCShellTestsTests(selftest.XPCShellTestsTests):
            def __init__(self, name):
                unittest.TestCase.__init__(self, name)
                self.testing_modules = this.testingModulesDir
                self.xpcshellBin = this.xpcshell
                self.utility_path = this.utility_path
                self.symbols_path = this.symbolsPath

        old_info = dict(mozinfo.info)
        try:
            suite = unittest.TestLoader().loadTestsFromTestCase(XPCShellTestsTests)
            return unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful()
        finally:
            # The self tests modify mozinfo, so we need to reset it.
            mozinfo.info.clear()
            mozinfo.update(old_info)

    def runTests(self, options, testClass=XPCShellTestThread, mobileArgs=None):
        """
        Run xpcshell tests.
        """

        global gotSIGINT

        # Number of times to repeat test(s) in --verify mode
        VERIFY_REPEAT = 10

        if isinstance(options, Namespace):
            options = vars(options)

        # Try to guess modules directory.
        # This somewhat grotesque hack allows the buildbot machines to find the
        # modules directory without having to configure the buildbot hosts. This
        # code path should never be executed in local runs because the build system
        # should always set this argument.
        if not options.get("testingModulesDir"):
            possible = os.path.join(here, os.path.pardir, "modules")

            if os.path.isdir(possible):
                testingModulesDir = possible

        if options.get("rerun_failures"):
            if os.path.exists(options.get("failure_manifest")):
                rerun_manifest = os.path.join(
                    os.path.dirname(options["failure_manifest"]), "rerun.ini"
                )
                shutil.copyfile(options["failure_manifest"], rerun_manifest)
                os.remove(options["failure_manifest"])
            else:
                self.log.error("No failures were found to re-run.")
                sys.exit(1)

        if options.get("testingModulesDir"):
            # The resource loader expects native paths. Depending on how we were
            # invoked, a UNIX style path may sneak in on Windows. We try to
            # normalize that.
            testingModulesDir = os.path.normpath(options["testingModulesDir"])

            if not os.path.isabs(testingModulesDir):
                testingModulesDir = os.path.abspath(testingModulesDir)

            if not testingModulesDir.endswith(os.path.sep):
                testingModulesDir += os.path.sep

        self.debuggerInfo = None

        if options.get("debugger"):
            self.debuggerInfo = mozdebug.get_debugger_info(
                options.get("debugger"),
                options.get("debuggerArgs"),
                options.get("debuggerInteractive"),
            )

        self.jsDebuggerInfo = None
        if options.get("jsDebugger"):
            # A namedtuple let's us keep .port instead of ['port']
            JSDebuggerInfo = namedtuple("JSDebuggerInfo", ["port"])
            self.jsDebuggerInfo = JSDebuggerInfo(port=options["jsDebuggerPort"])

        self.xpcshell = options.get("xpcshell")
        self.http3server = options.get("http3server")
        self.xrePath = options.get("xrePath")
        self.utility_path = options.get("utility_path")
        self.appPath = options.get("appPath")
        self.symbolsPath = options.get("symbolsPath")
        self.tempDir = os.path.normpath(options.get("tempDir") or tempfile.gettempdir())
        self.manifest = options.get("manifest")
        self.dump_tests = options.get("dump_tests")
        self.interactive = options.get("interactive")
        self.verbose = options.get("verbose")
        self.verboseIfFails = options.get("verboseIfFails")
        self.keepGoing = options.get("keepGoing")
        self.logfiles = options.get("logfiles")
        self.totalChunks = options.get("totalChunks", 1)
        self.thisChunk = options.get("thisChunk")
        self.profileName = options.get("profileName") or "xpcshell"
        self.mozInfo = options.get("mozInfo")
        self.testingModulesDir = testingModulesDir
        self.pluginsPath = options.get("pluginsPath")
        self.sequential = options.get("sequential")
        self.failure_manifest = options.get("failure_manifest")
        self.threadCount = options.get("threadCount") or NUM_THREADS
        self.jscovdir = options.get("jscovdir")
        self.enable_webrender = options.get("enable_webrender")
        self.headless = options.get("headless")
        self.runFailures = options.get("runFailures")
        self.timeoutAsPass = options.get("timeoutAsPass")
        self.crashAsPass = options.get("crashAsPass")

        self.testCount = 0
        self.passCount = 0
        self.failCount = 0
        self.todoCount = 0

        self.setAbsPath()
        prefs = self.buildPrefsFile(options.get("extraPrefs") or [])
        self.buildXpcsRunArgs()

        self.event = Event()

        if not self.updateMozinfo(prefs, options):
            return False

        if options.get("self_test"):
            if not self.runSelfTest():
                return False

        if (
            "tsan" in self.mozInfo
            and self.mozInfo["tsan"]
            and not options.get("threadCount")
        ):
            # TSan requires significantly more memory, so reduce the amount of parallel
            # tests we run to avoid OOMs and timeouts.
            # pylint --py3k W1619
            self.threadCount = self.threadCount / 2

        self.stack_fixer_function = None
        if self.utility_path and os.path.exists(self.utility_path):
            self.stack_fixer_function = get_stack_fixer_function(
                self.utility_path, self.symbolsPath
            )

        # buildEnvironment() needs mozInfo, so we call it after mozInfo is initialized.
        self.buildEnvironment()

        # The appDirKey is a optional entry in either the default or individual test
        # sections that defines a relative application directory for test runs. If
        # defined we pass 'grePath/$appDirKey' for the -a parameter of the xpcshell
        # test harness.
        appDirKey = None
        if "appname" in self.mozInfo:
            appDirKey = self.mozInfo["appname"] + "-appdir"

        # We have to do this before we run tests that depend on having the node
        # http/2 server.
        self.trySetupNode()

        self.startHttp3Server()

        pStdout, pStderr = self.getPipes()

        self.buildTestList(
            options.get("test_tags"), options.get("testPaths"), options.get("verify")
        )
        if self.singleFile:
            self.sequential = True

        if options.get("shuffle"):
            random.shuffle(self.alltests)

        self.cleanup_dir_list = []

        kwargs = {
            "appPath": self.appPath,
            "xrePath": self.xrePath,
            "utility_path": self.utility_path,
            "testingModulesDir": self.testingModulesDir,
            "debuggerInfo": self.debuggerInfo,
            "jsDebuggerInfo": self.jsDebuggerInfo,
            "pluginsPath": self.pluginsPath,
            "httpdJSPath": self.httpdJSPath,
            "headJSPath": self.headJSPath,
            "tempDir": self.tempDir,
            "testharnessdir": self.testharnessdir,
            "profileName": self.profileName,
            "singleFile": self.singleFile,
            "env": self.env,  # making a copy of this in the testthreads
            "symbolsPath": self.symbolsPath,
            "logfiles": self.logfiles,
            "xpcshell": self.xpcshell,
            "xpcsRunArgs": self.xpcsRunArgs,
            "failureManifest": self.failure_manifest,
            "jscovdir": self.jscovdir,
            "harness_timeout": self.harness_timeout,
            "stack_fixer_function": self.stack_fixer_function,
            "event": self.event,
            "cleanup_dir_list": self.cleanup_dir_list,
            "pStdout": pStdout,
            "pStderr": pStderr,
            "keep_going": self.keepGoing,
            "log": self.log,
            "interactive": self.interactive,
            "app_dir_key": appDirKey,
            "rootPrefsFile": self.prefsFile,
            "extraPrefs": options.get("extraPrefs") or [],
            "verboseIfFails": self.verboseIfFails,
            "headless": self.headless,
            "runFailures": self.runFailures,
            "timeoutAsPass": self.timeoutAsPass,
            "crashAsPass": self.crashAsPass,
        }

        if self.sequential:
            # Allow user to kill hung xpcshell subprocess with SIGINT
            # when we are only running tests sequentially.
            signal.signal(signal.SIGINT, markGotSIGINT)

        if self.debuggerInfo:
            # Force a sequential run
            self.sequential = True

            # If we have an interactive debugger, disable SIGINT entirely.
            if self.debuggerInfo.interactive:
                signal.signal(signal.SIGINT, lambda signum, frame: None)

            if "lldb" in self.debuggerInfo.path:
                # Ask people to start debugging using 'process launch', see bug 952211.
                self.log.info(
                    "It appears that you're using LLDB to debug this test.  "
                    + "Please use the 'process launch' command instead of "
                    "the 'run' command to start xpcshell."
                )

        if self.jsDebuggerInfo:
            # The js debugger magic needs more work to do the right thing
            # if debugging multiple files.
            if len(self.alltests) != 1:
                self.log.error(
                    "Error: --jsdebugger can only be used with a single test!"
                )
                return False

        # The test itself needs to know whether it is a tsan build, since
        # that has an effect on interpretation of the process return value.
        usingTSan = "tsan" in self.mozInfo and self.mozInfo["tsan"]

        # create a queue of all tests that will run
        tests_queue = deque()
        # also a list for the tests that need to be run sequentially
        sequential_tests = []
        status = None
        if not options.get("verify"):
            for test_object in self.alltests:
                # Test identifiers are provided for the convenience of logging. These
                # start as path names but are rewritten in case tests from the same path
                # are re-run.

                path = test_object["path"]

                if self.singleFile and not path.endswith(self.singleFile):
                    continue

                self.testCount += 1

                test = testClass(
                    test_object,
                    verbose=self.verbose or test_object.get("verbose") == "true",
                    usingTSan=usingTSan,
                    mobileArgs=mobileArgs,
                    **kwargs
                )
                if "run-sequentially" in test_object or self.sequential:
                    sequential_tests.append(test)
                else:
                    tests_queue.append(test)

            status = self.runTestList(
                tests_queue, sequential_tests, testClass, mobileArgs, **kwargs
            )
        else:
            #
            # Test verification: Run each test many times, in various configurations,
            # in hopes of finding intermittent failures.
            #

            def step1():
                # Run tests sequentially. Parallel mode would also work, except that
                # the logging system gets confused when 2 or more tests with the same
                # name run at the same time.
                sequential_tests = []
                for i in range(VERIFY_REPEAT):
                    self.testCount += 1
                    test = testClass(
                        test_object, retry=False, mobileArgs=mobileArgs, **kwargs
                    )
                    sequential_tests.append(test)
                status = self.runTestList(
                    tests_queue, sequential_tests, testClass, mobileArgs, **kwargs
                )
                return status

            def step2():
                # Run tests sequentially, with MOZ_CHAOSMODE enabled.
                sequential_tests = []
                self.env["MOZ_CHAOSMODE"] = "0xfb"
                for i in range(VERIFY_REPEAT):
                    self.testCount += 1
                    test = testClass(
                        test_object, retry=False, mobileArgs=mobileArgs, **kwargs
                    )
                    sequential_tests.append(test)
                status = self.runTestList(
                    tests_queue, sequential_tests, testClass, mobileArgs, **kwargs
                )
                return status

            steps = [
                ("1. Run each test %d times, sequentially." % VERIFY_REPEAT, step1),
                (
                    "2. Run each test %d times, sequentially, in chaos mode."
                    % VERIFY_REPEAT,
                    step2,
                ),
            ]
            startTime = datetime.now()
            maxTime = timedelta(seconds=options["verifyMaxTime"])
            for test_object in self.alltests:
                stepResults = {}
                for (descr, step) in steps:
                    stepResults[descr] = "not run / incomplete"
                finalResult = "PASSED"
                for (descr, step) in steps:
                    if (datetime.now() - startTime) > maxTime:
                        self.log.info(
                            "::: Test verification is taking too long: Giving up!"
                        )
                        self.log.info(
                            "::: So far, all checks passed, but not "
                            "all checks were run."
                        )
                        break
                    self.log.info(":::")
                    self.log.info('::: Running test verification step "%s"...' % descr)
                    self.log.info(":::")
                    status = step()
                    if status is not True:
                        stepResults[descr] = "FAIL"
                        finalResult = "FAILED!"
                        break
                    stepResults[descr] = "Pass"
                self.log.info(":::")
                self.log.info(
                    "::: Test verification summary for: %s" % test_object["path"]
                )
                self.log.info(":::")
                for descr in sorted(stepResults.keys()):
                    self.log.info("::: %s : %s" % (descr, stepResults[descr]))
                self.log.info(":::")
                self.log.info("::: Test verification %s" % finalResult)
                self.log.info(":::")

        self.shutdownNode()
        self.shutdownHttp3Server()

        return status

    def start_test(self, test):
        test.start()

    def test_ended(self, test):
        pass

    def runTestList(
        self, tests_queue, sequential_tests, testClass, mobileArgs, **kwargs
    ):

        if self.sequential:
            self.log.info("Running tests sequentially.")
        else:
            self.log.info("Using at most %d threads." % self.threadCount)

        # keep a set of threadCount running tests and start running the
        # tests in the queue at most threadCount at a time
        running_tests = set()
        keep_going = True
        exceptions = []
        tracebacks = []
        self.try_again_list = []

        tests_by_manifest = defaultdict(list)
        for test in self.alltests:
            group = test["manifest"]
            if "ancestor_manifest" in test:
                ancestor_manifest = normsep(test["ancestor_manifest"])
                # Only change the group id if ancestor is not the generated root manifest.
                if "/" in ancestor_manifest:
                    group = "{}:{}".format(ancestor_manifest, group)
            tests_by_manifest[group].append(test["id"])

        self.log.suite_start(tests_by_manifest, name="xpcshell")

        while tests_queue or running_tests:
            # if we're not supposed to continue and all of the running tests
            # are done, stop
            if not keep_going and not running_tests:
                break

            # if there's room to run more tests, start running them
            while (
                keep_going and tests_queue and (len(running_tests) < self.threadCount)
            ):
                test = tests_queue.popleft()
                running_tests.add(test)
                self.start_test(test)

            # queue is full (for now) or no more new tests,
            # process the finished tests so far

            # wait for at least one of the tests to finish
            self.event.wait(1)
            self.event.clear()

            # find what tests are done (might be more than 1)
            done_tests = set()
            for test in running_tests:
                if test.done:
                    self.test_ended(test)
                    done_tests.add(test)
                    test.join(
                        1
                    )  # join with timeout so we don't hang on blocked threads
                    # if the test had trouble, we will try running it again
                    # at the end of the run
                    if test.retry or test.is_alive():
                        # if the join call timed out, test.is_alive => True
                        self.try_again_list.append(test.test_object)
                        continue
                    # did the test encounter any exception?
                    if test.exception:
                        exceptions.append(test.exception)
                        tracebacks.append(test.traceback)
                        # we won't add any more tests, will just wait for
                        # the currently running ones to finish
                        keep_going = False
                    keep_going = keep_going and test.keep_going
                    self.addTestResults(test)

            # make room for new tests to run
            running_tests.difference_update(done_tests)

        if keep_going:
            # run the other tests sequentially
            for test in sequential_tests:
                if not keep_going:
                    self.log.error(
                        "TEST-UNEXPECTED-FAIL | Received SIGINT (control-C), so "
                        "stopped run. (Use --keep-going to keep running tests "
                        "after killing one with SIGINT)"
                    )
                    break
                # we don't want to retry these tests
                test.retry = False
                self.start_test(test)
                test.join()
                self.test_ended(test)
                self.addTestResults(test)
                # did the test encounter any exception?
                if test.exception:
                    exceptions.append(test.exception)
                    tracebacks.append(test.traceback)
                    break
                keep_going = test.keep_going

        # retry tests that failed when run in parallel
        if self.try_again_list:
            self.log.info("Retrying tests that failed when run in parallel.")
        for test_object in self.try_again_list:
            test = testClass(
                test_object,
                retry=False,
                verbose=self.verbose,
                mobileArgs=mobileArgs,
                **kwargs
            )
            self.start_test(test)
            test.join()
            self.test_ended(test)
            self.addTestResults(test)
            # did the test encounter any exception?
            if test.exception:
                exceptions.append(test.exception)
                tracebacks.append(test.traceback)
                break
            keep_going = test.keep_going

        # restore default SIGINT behaviour
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        # Clean up any slacker directories that might be lying around
        # Some might fail because of windows taking too long to unlock them.
        # We don't do anything if this fails because the test machines will have
        # their $TEMP dirs cleaned up on reboot anyway.
        for directory in self.cleanup_dir_list:
            try:
                shutil.rmtree(directory)
            except Exception:
                self.log.info("%s could not be cleaned up." % directory)

        if exceptions:
            self.log.info("Following exceptions were raised:")
            for t in tracebacks:
                self.log.error(t)
            raise exceptions[0]

        if self.testCount == 0 and os.environ.get("MOZ_AUTOMATION") != "1":
            self.log.error("No tests run. Did you pass an invalid --test-path?")
            self.failCount = 1

        # doing this allows us to pass the mozharness parsers that
        # report an orange job for failCount>0
        if self.runFailures:
            passed = self.passCount
            self.passCount = self.failCount
            self.failCount = passed

        self.log.info("INFO | Result summary:")
        self.log.info("INFO | Passed: %d" % self.passCount)
        self.log.info("INFO | Failed: %d" % self.failCount)
        self.log.info("INFO | Todo: %d" % self.todoCount)
        self.log.info("INFO | Retried: %d" % len(self.try_again_list))

        if gotSIGINT and not keep_going:
            self.log.error(
                "TEST-UNEXPECTED-FAIL | Received SIGINT (control-C), so stopped run. "
                "(Use --keep-going to keep running tests after "
                "killing one with SIGINT)"
            )
            return False

        self.log.suite_end()
        return self.runFailures or self.failCount == 0


def main():
    parser = parser_desktop()
    options = parser.parse_args()

    log = commandline.setup_logging("XPCShell", options, {"tbpl": sys.stdout})

    if options.xpcshell is None:
        log.error("Must provide path to xpcshell using --xpcshell")
        sys.exit(1)

    xpcsh = XPCShellTests(log)

    if options.interactive and not options.testPath:
        log.error("Error: You must specify a test filename in interactive mode!")
        sys.exit(1)

    if not xpcsh.runTests(options):
        sys.exit(1)


if __name__ == "__main__":
    main()
