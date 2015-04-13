#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import json
import math
import mozdebug
import mozinfo
import os
import os.path
import random
import re
import shutil
import signal
import sys
import time
import traceback

from collections import deque, namedtuple
from distutils import dir_util
from multiprocessing import cpu_count
from optparse import OptionParser
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

from automation import Automation

HARNESS_TIMEOUT = 5 * 60

# benchmarking on tbpl revealed that this works best for now
NUM_THREADS = int(cpu_count() * 4)

EXPECTED_LOG_ACTIONS = set([
    "test_status",
    "log",
])

# --------------------------------------------------------------
# TODO: this is a hack for mozbase without virtualenv, remove with bug 849900
#
here = os.path.dirname(__file__)
mozbase = os.path.realpath(os.path.join(os.path.dirname(here), 'mozbase'))

if os.path.isdir(mozbase):
    for package in os.listdir(mozbase):
        sys.path.append(os.path.join(mozbase, package))

from manifestparser import TestManifest
from manifestparser.filters import chunk_by_slice, tags
from mozlog import structured
import mozcrash
import mozinfo

# --------------------------------------------------------------

# TODO: perhaps this should be in a more generally shared location?
# This regex matches all of the C0 and C1 control characters
# (U+0000 through U+001F; U+007F; U+0080 through U+009F),
# except TAB (U+0009), CR (U+000D), LF (U+000A) and backslash (U+005C).
# A raw string is deliberately not used.
_cleanup_encoding_re = re.compile(u'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x9f\\\\]')
def _cleanup_encoding_repl(m):
    c = m.group(0)
    return '\\\\' if c == '\\' else '\\x{0:02X}'.format(ord(c))
def cleanup_encoding(s):
    """S is either a byte or unicode string.  Either way it may
       contain control characters, unpaired surrogates, reserved code
       points, etc.  If it is a byte string, it is assumed to be
       UTF-8, but it may not be *correct* UTF-8.  Return a
       sanitized unicode object."""
    if not isinstance(s, basestring):
        return unicode(s)
    if not isinstance(s, unicode):
        s = s.decode('utf-8', 'replace')
    # Replace all C0 and C1 control characters with \xNN escapes.
    return _cleanup_encoding_re.sub(_cleanup_encoding_repl, s)

""" Control-C handling """
gotSIGINT = False
def markGotSIGINT(signum, stackFrame):
    global gotSIGINT
    gotSIGINT = True

class XPCShellTestThread(Thread):
    def __init__(self, test_object, event, cleanup_dir_list, retry=True,
            tests_root_dir=None, app_dir_key=None, interactive=False,
            verbose=False, pStdout=None, pStderr=None, keep_going=False,
            log=None, **kwargs):
        Thread.__init__(self)
        self.daemon = True

        self.test_object = test_object
        self.cleanup_dir_list = cleanup_dir_list
        self.retry = retry

        self.appPath = kwargs.get('appPath')
        self.xrePath = kwargs.get('xrePath')
        self.testingModulesDir = kwargs.get('testingModulesDir')
        self.debuggerInfo = kwargs.get('debuggerInfo')
        self.jsDebuggerInfo = kwargs.get('jsDebuggerInfo')
        self.pluginsPath = kwargs.get('pluginsPath')
        self.httpdManifest = kwargs.get('httpdManifest')
        self.httpdJSPath = kwargs.get('httpdJSPath')
        self.headJSPath = kwargs.get('headJSPath')
        self.testharnessdir = kwargs.get('testharnessdir')
        self.profileName = kwargs.get('profileName')
        self.singleFile = kwargs.get('singleFile')
        self.env = copy.deepcopy(kwargs.get('env'))
        self.symbolsPath = kwargs.get('symbolsPath')
        self.logfiles = kwargs.get('logfiles')
        self.xpcshell = kwargs.get('xpcshell')
        self.xpcsRunArgs = kwargs.get('xpcsRunArgs')
        self.failureManifest = kwargs.get('failureManifest')

        self.tests_root_dir = tests_root_dir
        self.app_dir_key = app_dir_key
        self.interactive = interactive
        self.verbose = verbose
        self.pStdout = pStdout
        self.pStderr = pStderr
        self.keep_going = keep_going
        self.log = log

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
        self.complete_command = None
        self.harness_timeout = kwargs.get('harness_timeout')
        self.timedout = False

        # event from main thread to signal work done
        self.event = event
        self.done = False # explicitly set flag so we don't rely on thread.isAlive

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
            self.log.info("%s failed or timed out, will retry." %
                          self.test_object['id'])
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
        shutil.rmtree(dirname)

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
        return proc.returncode

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
        # timeout is needed by remote and b2g xpcshell to extend the
        # devicemanager.shell() timeout. It is not used in this function.
        if HAVE_PSUTIL:
            popen_func = psutil.Popen
        else:
            popen_func = Popen
        proc = popen_func(cmd, stdout=stdout, stderr=stderr,
                    env=env, cwd=cwd)
        return proc

    def checkForCrashes(self,
                        dump_directory,
                        symbols_path,
                        test_name=None):
        """
          Simple wrapper to check for crashes.
          On a remote system, this is more complex and we need to overload this function.
        """
        return mozcrash.check_for_crashes(dump_directory, symbols_path, test_name=test_name)

    def logCommand(self, name, completeCmd, testdir):
        self.log.info("%s | full command: %r" % (name, completeCmd))
        self.log.info("%s | current directory: %r" % (name, testdir))
        # Show only those environment variables that are changed from
        # the ambient environment.
        changedEnv = (set("%s=%s" % i for i in self.env.iteritems())
                      - set("%s=%s" % i for i in os.environ.iteritems()))
        self.log.info("%s | environment: %s" % (name, list(changedEnv)))

    def killTimeout(self, proc):
        Automation().killAndGetStackNoScreenshot(proc.pid,
                                                 self.appPath,
                                                 self.debuggerInfo)

    def postCheck(self, proc):
        """Checks for a still-running test process, kills it and fails the test if found.
        We can sometimes get here before the process has terminated, which would
        cause removeDir() to fail - so check for the process and kill it if needed.
        """
        if proc and self.poll(proc) is None:
            self.kill(proc)
            message = "%s | Process still running after test!" % self.test_object['id']
            if self.retry:
                self.log.info(message)
                self.log_full_output(self.output_lines)
                return

            self.log.error(message)
            self.log_full_output(self.output_lines)
            self.failCount = 1

    def testTimeout(self, proc):
        if self.test_object['expected'] == 'pass':
            expected = 'PASS'
        else:
            expected = 'FAIL'

        if self.retry:
            self.log.test_end(self.test_object['id'], 'TIMEOUT',
                              expected='TIMEOUT',
                              message="Test timed out")
        else:
            self.failCount = 1
            self.log.test_end(self.test_object['id'], 'TIMEOUT',
                              expected=expected,
                              message="Test timed out")
            self.log_full_output(self.output_lines)

        self.done = True
        self.timedout = True
        self.killTimeout(proc)
        self.log.info("xpcshell return code: %s" % self.getReturnCode(proc))
        self.postCheck(proc)
        self.clean_temp_dirs(self.test_object['path'])

    def buildCmdTestFile(self, name):
        """
          Build the command line arguments for the test file.
          On a remote system, this may be overloaded to use a remote path structure.
        """
        return ['-e', 'const _TEST_FILE = ["%s"];' %
                  name.replace('\\', '/')]

    def setupTempDir(self):
        tempDir = mkdtemp()
        self.env["XPCSHELL_TEST_TEMP_DIR"] = tempDir
        if self.interactive:
            self.log.info("temp dir is %s" % tempDir)
        return tempDir

    def setupPluginsDir(self):
        if not os.path.isdir(self.pluginsPath):
            return None

        pluginsDir = mkdtemp()
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
            except:
                pass
            os.makedirs(profileDir)
        else:
            profileDir = mkdtemp()
        self.env["XPCSHELL_TEST_PROFILE_DIR"] = profileDir
        if self.interactive or self.singleFile:
            self.log.info("profile dir is %s" % profileDir)
        return profileDir

    def buildCmdHead(self, headfiles, tailfiles, xpcscmd):
        """
          Build the command line arguments for the head and tail files,
          along with the address of the webserver which some tests require.

          On a remote system, this is overloaded to resolve quoting issues over a secondary command line.
        """
        cmdH = ", ".join(['"' + f.replace('\\', '/') + '"'
                       for f in headfiles])
        cmdT = ", ".join(['"' + f.replace('\\', '/') + '"'
                       for f in tailfiles])

        dbgport = 0 if self.jsDebuggerInfo is None else self.jsDebuggerInfo.port

        return xpcscmd + \
                ['-e', 'const _SERVER_ADDR = "localhost"',
                 '-e', 'const _HEAD_FILES = [%s];' % cmdH,
                 '-e', 'const _TAIL_FILES = [%s];' % cmdT,
                 '-e', 'const _JSDEBUGGER_PORT = %d;' % dbgport,
                ]

    def getHeadAndTailFiles(self, test_object):
        """Obtain the list of head and tail files.

        Returns a 2-tuple. The first element is a list of head files. The second
        is a list of tail files.
        """
        def sanitize_list(s, kind):
            for f in s.strip().split(' '):
                f = f.strip()
                if len(f) < 1:
                    continue

                path = os.path.normpath(os.path.join(test_object['here'], f))
                if not os.path.exists(path):
                    raise Exception('%s file does not exist: %s' % (kind, path))

                if not os.path.isfile(path):
                    raise Exception('%s file is not a file: %s' % (kind, path))

                yield path

        headlist = test_object['head'] if 'head' in test_object else ''
        taillist = test_object['tail'] if 'tail' in test_object else ''
        return (list(sanitize_list(headlist, 'head')),
                list(sanitize_list(taillist, 'tail')))

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
            self.xpcsCmd = [self.debuggerInfo.path] + self.debuggerInfo.args + self.xpcsCmd

        # Automation doesn't specify a pluginsPath and xpcshell defaults to
        # $APPDIR/plugins. We do the same here so we can carry on with
        # setting up every test with its own plugins directory.
        if not self.pluginsPath:
            self.pluginsPath = os.path.join(self.appPath, 'plugins')

        self.pluginsDir = self.setupPluginsDir()
        if self.pluginsDir:
            self.xpcsCmd.extend(['-p', self.pluginsDir])

    def cleanupDir(self, directory, name):
        if not os.path.exists(directory):
            return

        TRY_LIMIT = 25 # up to TRY_LIMIT attempts (one every second), because
                       # the Windows filesystem is slow to react to the changes
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

    def log_line(self, line):
        """Log a line of output (either a parser json object or text output from
        the test process"""
        if isinstance(line, basestring):
            line = cleanup_encoding(line).rstrip("\r\n")
            self.log.process_output(self.proc_ident,
                                    line,
                                    command=self.complete_command)
        else:
            if 'message' in line:
                line['message'] = cleanup_encoding(line['message'])
            if 'xpcshell_process' in line:
                line['thread'] =  ' '.join([current_thread().name, line['xpcshell_process']])
            else:
                line['thread'] = current_thread().name
            self.log.log_raw(line)

    def log_full_output(self, output):
        """Log output any buffered output from the test process"""
        if not output:
            return
        self.log.info(">>>>>>>")
        for line in output:
            self.log_line(line)
        self.log.info("<<<<<<<")

    def report_message(self, message):
        """Stores or logs a json log message in mozlog.structured
        format."""
        if self.verbose:
            self.log_line(message)
        else:
            # Tests eligible to retry will never dump their buffered output.
            if not self.retry:
                self.output_lines.append(message)

    def process_line(self, line_string):
        """ Parses a single line of output, determining its significance and
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

        if ('action' not in line_object or
            line_object['action'] not in EXPECTED_LOG_ACTIONS):
            # The test process output JSON.
            self.report_message(line_string)
            return

        action = line_object['action']

        self.has_failure_output = (self.has_failure_output or
                                   'expected' in line_object or
                                   action == 'log' and line_object['level'] == 'ERROR')

        self.report_message(line_object)

        if action == 'log' and line_object['message'] == 'CHILD-TEST-STARTED':
             self.saw_proc_start = True
        elif action == 'log' and line_object['message'] == 'CHILD-TEST-COMPLETED':
            self.saw_proc_end = True

    def run_test(self):
        """Run an individual xpcshell test."""
        global gotSIGINT

        name = self.test_object['id']
        path = self.test_object['path']

        # Check for skipped tests
        if 'disabled' in self.test_object:
            message = self.test_object['disabled']
            if not message:
                message = 'disabled from xpcshell manifest'
            self.log.test_start(name)
            self.log.test_end(name, 'SKIP', message=message)

            self.retry = False
            self.keep_going = True
            return

        # Check for known-fail tests
        expect_pass = self.test_object['expected'] == 'pass'

        # By default self.appPath will equal the gre dir. If specified in the
        # xpcshell.ini file, set a different app dir for this test.
        if self.app_dir_key and self.app_dir_key in self.test_object:
            rel_app_dir = self.test_object[self.app_dir_key]
            rel_app_dir = os.path.join(self.xrePath, rel_app_dir)
            self.appPath = os.path.abspath(rel_app_dir)
        else:
            self.appPath = None

        test_dir = os.path.dirname(path)
        self.buildXpcsCmd(test_dir)
        head_files, tail_files = self.getHeadAndTailFiles(self.test_object)
        cmdH = self.buildCmdHead(head_files, tail_files, self.xpcsCmd)

        # Create a profile and a temp dir that the JS harness can stick
        # a profile and temporary data in
        self.profileDir = self.setupProfileDir()
        self.tempDir = self.setupTempDir()

        # The test file will have to be loaded after the head files.
        cmdT = self.buildCmdTestFile(path)

        args = self.xpcsRunArgs[:]
        if 'debug' in self.test_object:
            args.insert(0, '-d')

        # The test name to log
        cmdI = ['-e', 'const _TEST_NAME = "%s"' % name]
        self.complete_command = cmdH + cmdT + cmdI + args

        if self.test_object.get('dmd') == 'true':
            if sys.platform.startswith('linux'):
                preloadEnvVar = 'LD_PRELOAD'
                libdmd = os.path.join(self.xrePath, 'libdmd.so')
            elif sys.platform == 'osx' or sys.platform == 'darwin':
                preloadEnvVar = 'DYLD_INSERT_LIBRARIES'
                # self.xrePath is <prefix>/Contents/Resources.
                # We need <prefix>/Contents/MacOS/libdmd.dylib.
                contents_dir = os.path.dirname(self.xrePath)
                libdmd = os.path.join(contents_dir, 'MacOS', 'libdmd.dylib')
            elif sys.platform == 'win32':
                preloadEnvVar = 'MOZ_REPLACE_MALLOC_LIB'
                libdmd = os.path.join(self.xrePath, 'dmd.dll')

            self.env['PYTHON'] = sys.executable
            self.env['BREAKPAD_SYMBOLS_PATH'] = self.symbolsPath
            self.env['DMD_PRELOAD_VAR'] = preloadEnvVar
            self.env['DMD_PRELOAD_VALUE'] = libdmd

        testTimeoutInterval = self.harness_timeout
        # Allow a test to request a multiple of the timeout if it is expected to take long
        if 'requesttimeoutfactor' in self.test_object:
            testTimeoutInterval *= int(self.test_object['requesttimeoutfactor'])

        testTimer = None
        if not self.interactive and not self.debuggerInfo and not self.jsDebuggerInfo:
            testTimer = Timer(testTimeoutInterval, lambda: self.testTimeout(proc))
            testTimer.start()

        proc = None
        process_output = None

        try:
            self.log.test_start(name)
            if self.verbose:
                self.logCommand(name, self.complete_command, test_dir)

            proc = self.launchProcess(self.complete_command,
                stdout=self.pStdout, stderr=self.pStderr, env=self.env, cwd=test_dir, timeout=testTimeoutInterval)

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
            passed = (not self.has_failure_output) and (return_code == 0)

            status = 'PASS' if passed else 'FAIL'
            expected = 'PASS' if expect_pass else 'FAIL'
            message = 'xpcshell return code: %d' % return_code

            if self.timedout:
                return

            if status != expected:
                if self.retry:
                    self.log.test_end(name, status, expected=status,
                                      message="Test failed or timed out, will retry")
                    self.clean_temp_dirs(path)
                    return

                self.log.test_end(name, status, expected=expected, message=message)
                self.log_full_output(self.output_lines)

                self.failCount += 1

                if self.failureManifest:
                    with open(self.failureManifest, 'a') as f:
                        f.write('[%s]\n' % self.test_object['path'])
                        for k, v in self.test_object.items():
                            f.write('%s = %s\n' % (k, v))

            else:
                self.log.test_end(name, status, expected=expected, message=message)
                if self.verbose:
                    self.log_full_output(self.output_lines)

                self.retry = False

                if expect_pass:
                    self.passCount = 1
                else:
                    self.todoCount = 1

            if self.checkForCrashes(self.tempDir, self.symbolsPath, test_name=name):
                if self.retry:
                    self.clean_temp_dirs(path)
                    return

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
        """ Initializes node status and logger. """
        self.log = log
        self.harness_timeout = HARNESS_TIMEOUT
        self.nodeProc = {}

    def buildTestList(self, test_tags=None):
        """
          read the xpcshell.ini manifest and set self.alltests to be
          an array of test objects.

          if we are chunking tests, it will be done here as well
        """
        if isinstance(self.manifest, TestManifest):
            mp = self.manifest
        else:
            mp = TestManifest(strict=True)
            if self.manifest is None:
                for testdir in self.testdirs:
                    if testdir:
                        mp.read(os.path.join(testdir, 'xpcshell.ini'))
            else:
                mp.read(self.manifest)

        self.buildTestPath()

        filters = []
        if test_tags:
            filters.append(tags(test_tags))

        if self.singleFile is None and self.totalChunks > 1:
            filters.append(chunk_by_slice(self.thisChunk, self.totalChunks))
        try:
            self.alltests = mp.active_tests(filters=filters, **mozinfo.info)
        except TypeError:
            sys.stderr.write("*** offending mozinfo.info: %s\n" % repr(mozinfo.info))
            raise

        if len(self.alltests) == 0:
            self.log.error("no tests to run using specified "
                           "combination of filters: {}".format(
                                mp.fmt_filters()))

    def setAbsPath(self):
        """
          Set the absolute path for xpcshell, httpdjspath and xrepath.
          These 3 variables depend on input from the command line and we need to allow for absolute paths.
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
                appBundlePath = os.path.join(os.path.dirname(os.path.dirname(self.xpcshell)), 'Resources')
                if os.path.exists(os.path.join(appBundlePath, 'application.ini')):
                    self.xrePath = appBundlePath
        else:
            self.xrePath = os.path.abspath(self.xrePath)

        # httpd.js belongs in xrePath/components, which is Contents/Resources on mac
        self.httpdJSPath = os.path.join(self.xrePath, 'components', 'httpd.js')
        self.httpdJSPath = self.httpdJSPath.replace('\\', '/')

        self.httpdManifest = os.path.join(self.xrePath, 'components', 'httpd.manifest')
        self.httpdManifest = self.httpdManifest.replace('\\', '/')

        if self.mozInfo is None:
            self.mozInfo = os.path.join(self.testharnessdir, "mozinfo.json")

    def buildCoreEnvironment(self):
        """
          Add environment variables likely to be used across all platforms, including remote systems.
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
        self.env.setdefault('MOZ_DISABLE_NONLOCAL_CONNECTIONS', '1')

    def buildEnvironment(self):
        """
          Create and returns a dictionary of self.env to include all the appropriate env variables and values.
          On a remote system, we overload this to set different values and are missing things like os.environ and PATH.
        """
        self.env = dict(os.environ)
        self.buildCoreEnvironment()
        if sys.platform == 'win32':
            self.env["PATH"] = self.env["PATH"] + ";" + self.xrePath
        elif sys.platform in ('os2emx', 'os2knix'):
            os.environ["BEGINLIBPATH"] = self.xrePath + ";" + self.env["BEGINLIBPATH"]
            os.environ["LIBPATHSTRICT"] = "T"
        elif sys.platform == 'osx' or sys.platform == "darwin":
            self.env["DYLD_LIBRARY_PATH"] = os.path.join(os.path.dirname(self.xrePath), 'MacOS')
        else: # unix or linux?
            if not "LD_LIBRARY_PATH" in self.env or self.env["LD_LIBRARY_PATH"] is None:
                self.env["LD_LIBRARY_PATH"] = self.xrePath
            else:
                self.env["LD_LIBRARY_PATH"] = ":".join([self.xrePath, self.env["LD_LIBRARY_PATH"]])

        if "asan" in self.mozInfo and self.mozInfo["asan"]:
            # ASan symbolizer support
            llvmsym = os.path.join(self.xrePath, "llvm-symbolizer")
            if os.path.isfile(llvmsym):
                self.env["ASAN_SYMBOLIZER_PATH"] = llvmsym
                self.log.info("runxpcshelltests.py | ASan using symbolizer at %s" % llvmsym)
            else:
                self.log.error("TEST-UNEXPECTED-FAIL | runxpcshelltests.py | Failed to find ASan symbolizer at %s" % llvmsym)

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
            if (self.debuggerInfo and self.debuggerInfo.interactive):
                pStdout = None
                pStderr = None
            else:
                if sys.platform == 'os2emx':
                    pStdout = None
                else:
                    pStdout = PIPE
                pStderr = STDOUT
        return pStdout, pStderr

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

    def verifyDirPath(self, dirname):
        """
          Simple wrapper to get the absolute path for a given directory name.
          On a remote system, we need to overload this to work on the remote filesystem.
        """
        return os.path.abspath(dirname)

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

            def startServer(name, serverJs):
                if os.path.exists(serverJs):
                    # OK, we found our SPDY server, let's try to get it running
                    self.log.info('Found %s at %s' % (name, serverJs))
                    try:
                        # We pipe stdin to node because the spdy server will exit when its
                        # stdin reaches EOF
                        process = Popen([nodeBin, serverJs], stdin=PIPE, stdout=PIPE,
                                stderr=PIPE, env=self.env, cwd=os.getcwd())
                        self.nodeProc[name] = process

                        # Check to make sure the server starts properly by waiting for it to
                        # tell us it's started
                        msg = process.stdout.readline()
                        if 'server listening' in msg:
                            nodeMozInfo['hasNode'] = True
                            searchObj = re.search( r'SPDY server listening on port (.*)', msg, 0)
                            if searchObj:
                              self.env["MOZSPDY-PORT"] = searchObj.group(1)
                            searchObj = re.search( r'HTTP2 server listening on port (.*)', msg, 0)
                            if searchObj:
                              self.env["MOZHTTP2-PORT"] = searchObj.group(1)
                    except OSError, e:
                        # This occurs if the subprocess couldn't be started
                        self.log.error('Could not run %s server: %s' % (name, str(e)))

            myDir = os.path.split(os.path.abspath(__file__))[0]
            startServer('moz-spdy', os.path.join(myDir, 'moz-spdy', 'moz-spdy.js'))
            startServer('moz-http2', os.path.join(myDir, 'moz-http2', 'moz-http2.js'))

        mozinfo.update(nodeMozInfo)

    def shutdownNode(self):
        """
          Shut down our node process, if it exists
        """
        for name, proc in self.nodeProc.iteritems():
            self.log.info('Node %s server shutting down ...' % name)
            if proc.poll() is not None:
                self.log.info('Node server %s already dead %s' % (name, proc.poll()))
            else:
                proc.terminate()
            def dumpOutput(fd, label):
                firstTime = True
                for msg in fd:
                    if firstTime:
                        firstTime = False;
                        self.log.info('Process %s' % label)
                    self.log.info(msg)
            dumpOutput(proc.stdout, "stdout")
            dumpOutput(proc.stderr, "stderr")

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

    def addTestResults(self, test):
        self.passCount += test.passCount
        self.failCount += test.failCount
        self.todoCount += test.todoCount

    def makeTestId(self, test_object):
        """Calculate an identifier for a test based on its path or a combination of
        its path and the source manifest."""

        relpath_key = 'file_relpath' if 'file_relpath' in test_object else 'relpath'
        path = test_object[relpath_key].replace('\\', '/');
        if 'dupe-manifest' in test_object and 'ancestor-manifest' in test_object:
            return '%s:%s' % (os.path.basename(test_object['ancestor-manifest']), path)
        return path

    def runTests(self, xpcshell, xrePath=None, appPath=None, symbolsPath=None,
                 manifest=None, testdirs=None, testPath=None, mobileArgs=None,
                 interactive=False, verbose=False, keepGoing=False, logfiles=True,
                 thisChunk=1, totalChunks=1, debugger=None,
                 debuggerArgs=None, debuggerInteractive=False,
                 profileName=None, mozInfo=None, sequential=False, shuffle=False,
                 testsRootDir=None, testingModulesDir=None, pluginsPath=None,
                 testClass=XPCShellTestThread, failureManifest=None,
                 log=None, stream=None, jsDebugger=False, jsDebuggerPort=0,
                 test_tags=None, **otherOptions):
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
        |pluginsPath|, if provided, custom plugins directory to be returned from
          the xpcshell dir svc provider for NS_APP_PLUGINS_DIR_LIST.
        |interactive|, if set to True, indicates to provide an xpcshell prompt
          instead of automatically executing the test.
        |verbose|, if set to True, will cause stdout/stderr from tests to
          be printed always
        |logfiles|, if set to False, indicates not to save output to log files.
          Non-interactive only option.
        |debugger|, if set, specifies the name of the debugger that will be used
          to launch xpcshell.
        |debuggerArgs|, if set, specifies arguments to use with the debugger.
        |debuggerInteractive|, if set, allows the debugger to be run in interactive
          mode.
        |profileName|, if set, specifies the name of the application for the profile
          directory if running only a subset of tests.
        |mozInfo|, if set, specifies specifies build configuration information, either as a filename containing JSON, or a dict.
        |shuffle|, if True, execute tests in random order.
        |testsRootDir|, absolute path to root directory of all tests. This is used
          by xUnit generation to determine the package name of the tests.
        |testingModulesDir|, if provided, specifies where JS modules reside.
          xpcshell will register a resource handler mapping this path.
        |otherOptions| may be present for the convenience of subclasses
        """

        global gotSIGINT

        if testdirs is None:
            testdirs = []

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

        self.debuggerInfo = None

        if debugger:
            self.debuggerInfo = mozdebug.get_debugger_info(debugger, debuggerArgs, debuggerInteractive)

        self.jsDebuggerInfo = None
        if jsDebugger:
            # A namedtuple let's us keep .port instead of ['port']
            JSDebuggerInfo = namedtuple('JSDebuggerInfo', ['port'])
            self.jsDebuggerInfo = JSDebuggerInfo(port=jsDebuggerPort)

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
        self.profileName = profileName or "xpcshell"
        self.mozInfo = mozInfo
        self.testingModulesDir = testingModulesDir
        self.pluginsPath = pluginsPath
        self.sequential = sequential

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

        self.event = Event()

        # Handle filenames in mozInfo
        if not isinstance(self.mozInfo, dict):
            mozInfoFile = self.mozInfo
            if not os.path.isfile(mozInfoFile):
                self.log.error("Error: couldn't find mozinfo.json at '%s'. Perhaps you need to use --build-info-json?" % mozInfoFile)
                return False
            self.mozInfo = json.load(open(mozInfoFile))

        # mozinfo.info is used as kwargs.  Some builds are done with
        # an older Python that can't handle Unicode keys in kwargs.
        # All of the keys in question should be ASCII.
        fixedInfo = {}
        for k, v in self.mozInfo.items():
            if isinstance(k, unicode):
                k = k.encode('ascii')
            fixedInfo[k] = v
        self.mozInfo = fixedInfo

        mozinfo.update(self.mozInfo)

        # buildEnvironment() needs mozInfo, so we call it after mozInfo is initialized.
        self.buildEnvironment()

        # The appDirKey is a optional entry in either the default or individual test
        # sections that defines a relative application directory for test runs. If
        # defined we pass 'grePath/$appDirKey' for the -a parameter of the xpcshell
        # test harness.
        appDirKey = None
        if "appname" in self.mozInfo:
            appDirKey = self.mozInfo["appname"] + "-appdir"

        # We have to do this before we build the test list so we know whether or
        # not to run tests that depend on having the node spdy server
        self.trySetupNode()

        pStdout, pStderr = self.getPipes()

        self.buildTestList(test_tags)
        if self.singleFile:
            self.sequential = True

        if shuffle:
            random.shuffle(self.alltests)

        self.cleanup_dir_list = []
        self.try_again_list = []

        kwargs = {
            'appPath': self.appPath,
            'xrePath': self.xrePath,
            'testingModulesDir': self.testingModulesDir,
            'debuggerInfo': self.debuggerInfo,
            'jsDebuggerInfo': self.jsDebuggerInfo,
            'pluginsPath': self.pluginsPath,
            'httpdManifest': self.httpdManifest,
            'httpdJSPath': self.httpdJSPath,
            'headJSPath': self.headJSPath,
            'testharnessdir': self.testharnessdir,
            'profileName': self.profileName,
            'singleFile': self.singleFile,
            'env': self.env, # making a copy of this in the testthreads
            'symbolsPath': self.symbolsPath,
            'logfiles': self.logfiles,
            'xpcshell': self.xpcshell,
            'xpcsRunArgs': self.xpcsRunArgs,
            'failureManifest': failureManifest,
            'harness_timeout': self.harness_timeout,
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
                self.log.info("It appears that you're using LLDB to debug this test.  " +
                              "Please use the 'process launch' command instead of the 'run' command to start xpcshell.")

        if self.jsDebuggerInfo:
            # The js debugger magic needs more work to do the right thing
            # if debugging multiple files.
            if len(self.alltests) != 1:
                self.log.error("Error: --jsdebugger can only be used with a single test!")
                return False

        # create a queue of all tests that will run
        tests_queue = deque()
        # also a list for the tests that need to be run sequentially
        sequential_tests = []
        for test_object in self.alltests:
            # Test identifiers are provided for the convenience of logging. These
            # start as path names but are rewritten in case tests from the same path
            # are re-run.

            path = test_object['path']
            test_object['id'] = self.makeTestId(test_object)

            if self.singleFile and not path.endswith(self.singleFile):
                continue

            if self.testPath and path.find(self.testPath) == -1:
                continue

            self.testCount += 1

            test = testClass(test_object, self.event, self.cleanup_dir_list,
                    tests_root_dir=testsRootDir, app_dir_key=appDirKey,
                    interactive=interactive, verbose=verbose, pStdout=pStdout,
                    pStderr=pStderr, keep_going=keepGoing, log=self.log,
                    mobileArgs=mobileArgs, **kwargs)
            if 'run-sequentially' in test_object or self.sequential:
                sequential_tests.append(test)
            else:
                tests_queue.append(test)

        if self.sequential:
            self.log.info("Running tests sequentially.")
        else:
            self.log.info("Using at most %d threads." % NUM_THREADS)

        # keep a set of NUM_THREADS running tests and start running the
        # tests in the queue at most NUM_THREADS at a time
        running_tests = set()
        keep_going = True
        exceptions = []
        tracebacks = []
        self.log.suite_start([t['id'] for t in self.alltests])

        while tests_queue or running_tests:
            # if we're not supposed to continue and all of the running tests
            # are done, stop
            if not keep_going and not running_tests:
                break

            # if there's room to run more tests, start running them
            while keep_going and tests_queue and (len(running_tests) < NUM_THREADS):
                test = tests_queue.popleft()
                running_tests.add(test)
                test.start()

            # queue is full (for now) or no more new tests,
            # process the finished tests so far

            # wait for at least one of the tests to finish
            self.event.wait(1)
            self.event.clear()

            # find what tests are done (might be more than 1)
            done_tests = set()
            for test in running_tests:
                if test.done:
                    done_tests.add(test)
                    test.join(1) # join with timeout so we don't hang on blocked threads
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
                    self.log.error("TEST-UNEXPECTED-FAIL | Received SIGINT (control-C), so stopped run. " \
                                   "(Use --keep-going to keep running tests after killing one with SIGINT)")
                    break
                # we don't want to retry these tests
                test.retry = False
                test.start()
                test.join()
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
            test = testClass(test_object, self.event, self.cleanup_dir_list,
                    retry=False, tests_root_dir=testsRootDir,
                    app_dir_key=appDirKey, interactive=interactive,
                    verbose=verbose, pStdout=pStdout, pStderr=pStderr,
                    keep_going=keepGoing, log=self.log, mobileArgs=mobileArgs,
                    **kwargs)
            test.start()
            test.join()
            self.addTestResults(test)
            # did the test encounter any exception?
            if test.exception:
                exceptions.append(test.exception)
                tracebacks.append(test.traceback)
                break
            keep_going = test.keep_going

        # restore default SIGINT behaviour
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        self.shutdownNode()
        # Clean up any slacker directories that might be lying around
        # Some might fail because of windows taking too long to unlock them.
        # We don't do anything if this fails because the test slaves will have
        # their $TEMP dirs cleaned up on reboot anyway.
        for directory in self.cleanup_dir_list:
            try:
                shutil.rmtree(directory)
            except:
                self.log.info("%s could not be cleaned up." % directory)

        if exceptions:
            self.log.info("Following exceptions were raised:")
            for t in tracebacks:
                self.log.error(t)
            raise exceptions[0]

        if self.testCount == 0:
            self.log.error("No tests run. Did you pass an invalid --test-path?")
            self.failCount = 1

        self.log.info("INFO | Result summary:")
        self.log.info("INFO | Passed: %d" % self.passCount)
        self.log.info("INFO | Failed: %d" % self.failCount)
        self.log.info("INFO | Todo: %d" % self.todoCount)
        self.log.info("INFO | Retried: %d" % len(self.try_again_list))

        if gotSIGINT and not keepGoing:
            self.log.error("TEST-UNEXPECTED-FAIL | Received SIGINT (control-C), so stopped run. " \
                           "(Use --keep-going to keep running tests after killing one with SIGINT)")
            return False

        self.log.suite_end()
        return self.failCount == 0

class XPCShellOptions(OptionParser):
    def __init__(self):
        """Process command line arguments and call runTests() to do the real work."""
        OptionParser.__init__(self)
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
        self.add_option("--sequential",
                        action="store_true", dest="sequential", default=False,
                        help="Run all tests sequentially")
        self.add_option("--test-path",
                        type="string", dest="testPath", default=None,
                        help="single path and/or test filename to test")
        self.add_option("--tests-root-dir",
                        type="string", dest="testsRootDir", default=None,
                        help="absolute path to directory where all tests are located. this is typically $(objdir)/_tests")
        self.add_option("--testing-modules-dir",
                        dest="testingModulesDir", default=None,
                        help="Directory where testing modules are located.")
        self.add_option("--test-plugin-path",
                        type="string", dest="pluginsPath", default=None,
                        help="Path to the location of a plugins directory containing the test plugin or plugins required for tests. "
                             "By default xpcshell's dir svc provider returns gre/plugins. Use test-plugin-path to add a directory "
                             "to return for NS_APP_PLUGINS_DIR_LIST when queried.")
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
        self.add_option("--failure-manifest", dest="failureManifest",
                        action="store",
                        help="path to file where failure manifest will be written.")
        self.add_option("--xre-path",
                        action = "store", type = "string", dest = "xrePath",
                        # individual scripts will set a sane default
                        default = None,
                        help = "absolute path to directory containing XRE (probably xulrunner)")
        self.add_option("--symbols-path",
                        action = "store", type = "string", dest = "symbolsPath",
                        default = None,
                        help = "absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols")
        self.add_option("--debugger",
                        action = "store", dest = "debugger",
                        help = "use the given debugger to launch the application")
        self.add_option("--debugger-args",
                        action = "store", dest = "debuggerArgs",
                        help = "pass the given args to the debugger _before_ "
                           "the application on the command line")
        self.add_option("--debugger-interactive",
                        action = "store_true", dest = "debuggerInteractive",
                        help = "prevents the test harness from redirecting "
                          "stdout and stderr for interactive debuggers")
        self.add_option("--jsdebugger", dest="jsDebugger", action="store_true",
                        help="Waits for a devtools JS debugger to connect before "
                             "starting the test.")
        self.add_option("--jsdebugger-port", type="int", dest="jsDebuggerPort",
                        default=6000,
                        help="The port to listen on for a debugger connection if "
                             "--jsdebugger is specified.")
        self.add_option("--tag",
                        action="append", dest="test_tags",
                        default=None,
                        help="filter out tests that don't have the given tag. Can be "
                             "used multiple times in which case the test must contain "
                             "at least one of the given tags.")

def main():
    parser = XPCShellOptions()
    structured.commandline.add_logging_group(parser)
    options, args = parser.parse_args()


    log = structured.commandline.setup_logging("XPCShell",
                                               options,
                                               {"tbpl": sys.stdout})

    if len(args) < 2 and options.manifest is None or \
       (len(args) < 1 and options.manifest is not None):
        print >>sys.stderr, """Usage: %s <path to xpcshell> <test dirs>
              or: %s --manifest=test.manifest <path to xpcshell>""" % (sys.argv[0],
                                                              sys.argv[0])
        sys.exit(1)

    xpcsh = XPCShellTests(log)

    if options.interactive and not options.testPath:
        print >>sys.stderr, "Error: You must specify a test filename in interactive mode!"
        sys.exit(1)

    if not xpcsh.runTests(args[0], testdirs=args[1:], **options.__dict__):
        sys.exit(1)

if __name__ == '__main__':
    main()
