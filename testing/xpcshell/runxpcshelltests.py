#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import json
import math
import os
import os.path
import random
import shutil
import signal
import socket
import sys
import time
import traceback
import xml.dom.minidom
from collections import deque
from distutils import dir_util
from multiprocessing import cpu_count
from optparse import OptionParser
from subprocess import Popen, PIPE, STDOUT
from tempfile import mkdtemp, gettempdir
from threading import Timer, Thread, Event, RLock

try:
    import psutil
    HAVE_PSUTIL = True
except ImportError:
    HAVE_PSUTIL = False

from automation import Automation, getGlobalLog, resetGlobalLog
from automationutils import *

# Printing buffered output in case of a failure or verbose mode will result
# in buffered output interleaved with other threads' output.
# To prevent his, each call to the logger as well as any blocks of output that
# are intended to be continuous are protected by the same lock.
LOG_MUTEX = RLock()

HARNESS_TIMEOUT = 5 * 60

# benchmarking on tbpl revealed that this works best for now
NUM_THREADS = int(cpu_count() * 4)

FAILURE_ACTIONS = set(['test_unexpected_fail',
                       'test_unexpected_pass',
                       'javascript_error'])
ACTION_STRINGS = {
    "test_unexpected_fail": "TEST-UNEXPECTED-FAIL",
    "test_known_fail": "TEST-KNOWN-FAIL",
    "test_unexpected_pass": "TEST-UNEXPECTED-PASS",
    "javascript_error": "TEST-UNEXPECTED-FAIL",
    "test_pass": "TEST-PASS",
    "test_info": "TEST-INFO"
}

# --------------------------------------------------------------
# TODO: this is a hack for mozbase without virtualenv, remove with bug 849900
#
here = os.path.dirname(__file__)
mozbase = os.path.realpath(os.path.join(os.path.dirname(here), 'mozbase'))

if os.path.isdir(mozbase):
    for package in os.listdir(mozbase):
        sys.path.append(os.path.join(mozbase, package))

import manifestparser
import mozcrash
import mozinfo

# ---------------------------------------------------------------
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

        self.output_lines = []
        self.has_failure_output = False

        # event from main thread to signal work done
        self.event = event

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
            self.log.info("TEST-INFO | %s | Test failed or timed out, will retry."
                          % self.test_object['name'])
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
        return proc.communicate()

    def launchProcess(self, cmd, stdout, stderr, env, cwd):
        """
          Simple wrapper to launch a process.
          On a remote system, this is more complex and we need to overload this function.
        """
        if HAVE_PSUTIL:
            popen_func = psutil.Popen
        else:
            popen_func = Popen
        proc = popen_func(cmd, stdout=stdout, stderr=stderr,
                    env=env, cwd=cwd)
        return proc

    def logCommand(self, name, completeCmd, testdir):
        self.log.info("TEST-INFO | %s | full command: %r" % (name, completeCmd))
        self.log.info("TEST-INFO | %s | current directory: %r" % (name, testdir))
        # Show only those environment variables that are changed from
        # the ambient environment.
        changedEnv = (set("%s=%s" % i for i in self.env.iteritems())
                      - set("%s=%s" % i for i in os.environ.iteritems()))
        self.log.info("TEST-INFO | %s | environment: %s" % (name, list(changedEnv)))

    def testTimeout(self, test_file, processPID):
        if not self.retry:
            self.log.error("TEST-UNEXPECTED-FAIL | %s | Test timed out" % test_file)
        Automation().killAndGetStackNoScreenshot(processPID, self.appPath, self.debuggerInfo)

    def buildCmdTestFile(self, name):
        """
          Build the command line arguments for the test file.
          On a remote system, this may be overloaded to use a remote path structure.
        """
        return ['-e', 'const _TEST_FILE = ["%s"];' %
                  replaceBackSlashes(name)]

    def setupTempDir(self):
        tempDir = mkdtemp()
        self.env["XPCSHELL_TEST_TEMP_DIR"] = tempDir
        if self.interactive:
            self.log.info("TEST-INFO | temp dir is %s" % tempDir)
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
            self.log.info("TEST-INFO | plugins dir is %s" % pluginsDir)
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
            self.log.info("TEST-INFO | profile dir is %s" % profileDir)
        return profileDir

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

        return (list(sanitize_list(test_object['head'], 'head')),
                list(sanitize_list(test_object['tail'], 'tail')))

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

        # Automation doesn't specify a pluginsPath and xpcshell defaults to
        # $APPDIR/plugins. We do the same here so we can carry on with
        # setting up every test with its own plugins directory.
        if not self.pluginsPath:
            self.pluginsPath = os.path.join(self.appPath, 'plugins')

        self.pluginsDir = self.setupPluginsDir()
        if self.pluginsDir:
            self.xpcsCmd.extend(['-p', self.pluginsDir])

    def cleanupDir(self, directory, name, xunit_result):
        if not os.path.exists(directory):
            return

        TRY_LIMIT = 25 # up to TRY_LIMIT attempts (one every second), because
                       # the Windows filesystem is slow to react to the changes
        try_count = 0
        while try_count < TRY_LIMIT:
            try:
                self.removeDir(directory)
            except OSError:
                self.log.info("TEST-INFO | Failed to remove directory: %s. Waiting." % directory)
                # We suspect the filesystem may still be making changes. Wait a
                # little bit and try again.
                time.sleep(1)
                try_count += 1
            else:
                # removed fine
                return

        # we try again later at the end of the run for plugin dirs (because windows!)
        if directory == self.pluginsDir:
            self.cleanup_dir_list.append(directory)
            return

        # we couldn't clean up the directory, and it's not the plugins dir,
        # which means that something wrong has probably happened
        if self.retry:
            return

        with LOG_MUTEX:
            message = "TEST-UNEXPECTED-FAIL | %s | Failed to clean up directory: %s" % (name, sys.exc_info()[1])
            self.log.error(message)
            self.log_output(self.output_lines)
            self.log_output(traceback.format_exc())

        self.failCount += 1
        xunit_result["passed"] = False
        xunit_result["failure"] = {
            "type": "TEST-UNEXPECTED-FAIL",
            "message": message,
            "text": "%s\n%s" % ("\n".join(self.output_lines), traceback.format_exc())
        }

    def clean_temp_dirs(self, name, stdout):
        # We don't want to delete the profile when running check-interactive
        # or check-one.
        if self.profileDir and not self.interactive and not self.singleFile:
            self.cleanupDir(self.profileDir, name, self.xunit_result)

        self.cleanupDir(self.tempDir, name, self.xunit_result)

        if self.pluginsDir:
            self.cleanupDir(self.pluginsDir, name, self.xunit_result)

    def append_message_from_line(self, line):
        """Given a line of raw output, convert to message and append to
        output buffer."""
        if isinstance(line, basestring):
            # This function has received unstructured output.
            if line:
                self.output_lines.append(line)
                if 'TEST-UNEXPECTED-' in line:
                    self.has_failure_output = True
            return

        msg = ['%s: ' % line['process'] if 'process' in line else '']

        # Each call to the logger in head.js either specified '_message'
        # or both 'source_file' and 'diagnostic'. If either of these are
        # missing, they ended up being undefined as a result of the way
        # the test was run.
        if '_message' in line:
            msg.append(line['_message'])
        else:
            msg.append('%s | %s | %s' % (ACTION_STRINGS[line['action']],
                                         line.get('source_file', 'undefined'),
                                         line.get('diagnostic', 'undefined')))

        msg.append('\n%s' % line['stack'] if 'stack' in line else '')
        self.output_lines.append(''.join(msg))

    def parse_output(self, output):
        """Parses process output for structured messages and saves output as it is
        read. Sets self.has_failure_output in case of evidence of a failure"""
        seen_proc_start = False
        seen_proc_end = False
        self.output_lines = []
        for line_string in output.splitlines():
            try:
                line_object = json.loads(line_string)
                if not isinstance(line_object, dict):
                    self.append_message_from_line(line_string)
                    continue
            except ValueError:
                self.append_message_from_line(line_string)
                continue

            if 'action' not in line_object:
                # In case a test outputs something that happens to be valid
                # JSON object.
                self.append_message_from_line(line_string)
                continue

            action = line_object['action']
            self.append_message_from_line(line_object)

            if action in FAILURE_ACTIONS:
                self.has_failure_output = True

            elif action == 'child_test_start':
                seen_proc_start = True
            elif action == 'child_test_end':
                seen_proc_end = True

        if seen_proc_start and not seen_proc_end:
            self.has_failure_output = True

    def log_output(self, output):
        """Prints given output line-by-line to avoid overflowing buffers."""
        self.log.info(">>>>>>>")
        if output:
            if isinstance(output, basestring):
                output = output.splitlines()
            for part in output:
                # For multi-line output, such as a stack trace
                for line in part.splitlines():
                    self.log.info(line)
        self.log.info("<<<<<<<")

    def run_test(self):
        """Run an individual xpcshell test."""
        global gotSIGINT

        name = self.test_object['path']

        self.xunit_result = {'name': self.test_object['name'], 'classname': 'xpcshell'}

        # The xUnit package is defined as the path component between the root
        # dir and the test with path characters replaced with '.' (using Java
        # class notation).
        if self.tests_root_dir is not None:
            self.tests_root_dir = os.path.normpath(self.tests_root_dir)
            if self.test_object['here'].find(self.tests_root_dir) != 0:
                raise Exception('tests_root_dir is not a parent path of %s' %
                    self.test_object['here'])
            relpath = self.test_object['here'][len(self.tests_root_dir):].lstrip('/\\')
            self.xunit_result['classname'] = relpath.replace('/', '.').replace('\\', '.')

        # Check for skipped tests
        if 'disabled' in self.test_object:
            self.log.info('TEST-INFO | skipping %s | %s' %
                (name, self.test_object['disabled']))

            self.xunit_result['skipped'] = True
            self.retry = False

            self.keep_going = True
            return

        # Check for known-fail tests
        expected = self.test_object['expected'] == 'pass'

        # By default self.appPath will equal the gre dir. If specified in the
        # xpcshell.ini file, set a different app dir for this test.
        if self.app_dir_key and self.app_dir_key in self.test_object:
            rel_app_dir = self.test_object[self.app_dir_key]
            rel_app_dir = os.path.join(self.xrePath, rel_app_dir)
            self.appPath = os.path.abspath(rel_app_dir)
        else:
            self.appPath = None

        test_dir = os.path.dirname(name)
        self.buildXpcsCmd(test_dir)
        head_files, tail_files = self.getHeadAndTailFiles(self.test_object)
        cmdH = self.buildCmdHead(head_files, tail_files, self.xpcsCmd)

        # Create a profile and a temp dir that the JS harness can stick
        # a profile and temporary data in
        self.profileDir = self.setupProfileDir()
        self.tempDir = self.setupTempDir()

        # The test file will have to be loaded after the head files.
        cmdT = self.buildCmdTestFile(name)

        args = self.xpcsRunArgs[:]
        if 'debug' in self.test_object:
            args.insert(0, '-d')

        completeCmd = cmdH + cmdT + args

        testTimer = None
        if not self.interactive and not self.debuggerInfo:
            testTimer = Timer(HARNESS_TIMEOUT, lambda: self.testTimeout(name, proc.pid))
            testTimer.start()

        proc = None
        stdout = None
        stderr = None

        try:
            self.log.info("TEST-INFO | %s | running test ..." % name)
            if self.verbose:
                self.logCommand(name, completeCmd, test_dir)

            startTime = time.time()
            proc = self.launchProcess(completeCmd,
                stdout=self.pStdout, stderr=self.pStderr, env=self.env, cwd=test_dir)

            if self.interactive:
                self.log.info("TEST-INFO | %s | Process ID: %d" % (name, proc.pid))

            stdout, stderr = self.communicate(proc)

            if self.interactive:
                # Not sure what else to do here...
                self.keep_going = True
                return

            if testTimer:
                testTimer.cancel()

            if stdout:
                self.parse_output(stdout)
            result = not (self.has_failure_output or
                          (self.getReturnCode(proc) != 0))

            if result != expected:
                if self.retry:
                    self.clean_temp_dirs(name, stdout)
                    return

                failureType = "TEST-UNEXPECTED-%s" % ("FAIL" if expected else "PASS")
                message = "%s | %s | test failed (with xpcshell return code: %d), see following log:" % (
                              failureType, name, self.getReturnCode(proc))

                with LOG_MUTEX:
                    self.log.error(message)
                    self.log_output(self.output_lines)

                self.failCount += 1
                self.xunit_result["passed"] = False

                self.xunit_result["failure"] = {
                  "type": failureType,
                  "message": message,
                  "text": stdout
                }

                if self.failureManifest:
                    with open(self.failureManifest, 'a') as f:
                        f.write('[%s]\n' % self.test_object['path'])
                        for k, v in self.test_object.items():
                            f.write('%s = %s\n' % (k, v))

            else:
                now = time.time()
                timeTaken = (now - startTime) * 1000
                self.xunit_result["time"] = now - startTime

                with LOG_MUTEX:
                    self.log.info("TEST-%s | %s | test passed (time: %.3fms)" % ("PASS" if expected else "KNOWN-FAIL", name, timeTaken))
                    if self.verbose:
                        self.log_output(self.output_lines)

                self.xunit_result["passed"] = True
                self.retry = False

                if expected:
                    self.passCount = 1
                else:
                    self.todoCount = 1
                    self.xunit_result["todo"] = True

            if mozcrash.check_for_crashes(self.tempDir, self.symbolsPath, test_name=name):
                if self.retry:
                    self.clean_temp_dirs(name, stdout)
                    return

                message = "PROCESS-CRASH | %s | application crashed" % name
                self.failCount = 1
                self.xunit_result["passed"] = False
                self.xunit_result["failure"] = {
                    "type": "PROCESS-CRASH",
                    "message": message,
                    "text": stdout
                }

            if self.logfiles and stdout:
                self.createLogFile(name, stdout)

        finally:
            # We can sometimes get here before the process has terminated, which would
            # cause removeDir() to fail - so check for the process & kill it it needed.
            if proc and self.poll(proc) is None:
                self.kill(proc)

                if self.retry:
                    self.clean_temp_dirs(name, stdout)
                    return

                with LOG_MUTEX:
                    message = "TEST-UNEXPECTED-FAIL | %s | Process still running after test!" % name
                    self.log.error(message)
                    self.log_output(self.output_lines)

                self.failCount = 1
                self.xunit_result["passed"] = False
                self.xunit_result["failure"] = {
                  "type": "TEST-UNEXPECTED-FAIL",
                  "message": message,
                  "text": stdout
                }

            self.clean_temp_dirs(name, stdout)

        if gotSIGINT:
            self.xunit_result["passed"] = False
            self.xunit_result["time"] = "0.0"
            self.xunit_result["failure"] = {
                "type": "SIGINT",
                "message": "Received SIGINT",
                "text": "Received SIGINT (control-C) during test execution."
            }

            self.log.error("TEST-UNEXPECTED-FAIL | Received SIGINT (control-C) during test execution")
            if self.keep_going:
                gotSIGINT = False
            else:
                self.keep_going = False
                return

        self.keep_going = True

class XPCShellTests(object):

    log = getGlobalLog()
    oldcwd = os.getcwd()

    def __init__(self, log=None):
        """ Init logging and node status """
        if log:
            resetGlobalLog(log)

        # Each method of the underlying logger must acquire the log
        # mutex before writing to stdout.
        log_funs = ['debug', 'info', 'warning', 'error', 'critical', 'log']
        for fun_name in log_funs:
            unwrapped = getattr(self.log, fun_name, None)
            if unwrapped:
                def wrap(fn):
                    def wrapped(*args, **kwargs):
                        with LOG_MUTEX:
                            fn(*args, **kwargs)
                    return wrapped
                setattr(self.log, fun_name, wrap(unwrapped))

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

    def buildCoreEnvironment(self):
        """
          Add environment variables likely to be used across all platforms, including remote systems.
        """
        # Make assertions fatal
        self.env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
        # Enable crash reporting
        self.env["MOZ_CRASHREPORTER"] = "1"
        # Don't launch the crash reporter client
        self.env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        # Capturing backtraces is very slow on some platforms, and it's
        # disabled by automation.py too
        self.env["NS_TRACE_MALLOC_DISABLE_STACKS"] = "1"

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
            self.env["DYLD_LIBRARY_PATH"] = self.xrePath
        else: # unix or linux?
            if not "LD_LIBRARY_PATH" in self.env or self.env["LD_LIBRARY_PATH"] is None:
                self.env["LD_LIBRARY_PATH"] = self.xrePath
            else:
                self.env["LD_LIBRARY_PATH"] = ":".join([self.xrePath, self.env["LD_LIBRARY_PATH"]])

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
            assert isinstance(name, basestring)

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

    def post_to_autolog(self, results, name):
        from moztest.results import TestContext, TestResult, TestResultCollection
        from moztest.output.autolog import AutologOutput

        context = TestContext(
            testgroup='b2g xpcshell testsuite',
            operating_system='android',
            arch='emulator',
            harness='xpcshell',
            hostname=socket.gethostname(),
            tree='b2g',
            buildtype='opt',
            )

        collection = TestResultCollection('b2g emulator testsuite')

        for result in results:
            duration = result.get('time', 0)

            if 'skipped' in result:
                outcome = 'SKIPPED'
            elif 'todo' in result:
                outcome = 'KNOWN-FAIL'
            elif result['passed']:
                outcome = 'PASS'
            else:
                outcome = 'UNEXPECTED-FAIL'

            output = None
            if 'failure' in result:
                output = result['failure']['text']

            t = TestResult(name=result['name'], test_class=name,
                           time_start=0, context=context)
            t.finish(result=outcome, time_end=duration, output=output)

            collection.append(t)
            collection.time_taken += duration

        out = AutologOutput()
        out.post(out.make_testgroups(collection))

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
        self.xunitResults.append(test.xunit_result)

    def runTests(self, xpcshell, xrePath=None, appPath=None, symbolsPath=None,
                 manifest=None, testdirs=None, testPath=None, mobileArgs=None,
                 interactive=False, verbose=False, keepGoing=False, logfiles=True,
                 thisChunk=1, totalChunks=1, debugger=None,
                 debuggerArgs=None, debuggerInteractive=False,
                 profileName=None, mozInfo=None, sequential=False, shuffle=False,
                 testsRootDir=None, xunitFilename=None, xunitName=None,
                 testingModulesDir=None, autolog=False, pluginsPath=None,
                 testClass=XPCShellTestThread, failureManifest=None,
                 **otherOptions):
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
            if not isinstance(testsRootDir, basestring):
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
        self.buildEnvironment()

        self.event = Event()

        # Handle filenames in mozInfo
        if not isinstance(self.mozInfo, dict):
            mozInfoFile = self.mozInfo
            if not os.path.isfile(mozInfoFile):
                self.log.error("Error: couldn't find mozinfo.json at '%s'. Perhaps you need to use --build-info-json?" % mozInfoFile)
                return False
            self.mozInfo = parse_json(open(mozInfoFile).read())
        mozinfo.update(self.mozInfo)

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

        self.buildTestList()
        if self.singleFile:
            self.sequential = True

        if shuffle:
            random.shuffle(self.alltests)

        self.xunitResults = []
        self.cleanup_dir_list = []
        self.try_again_list = []

        kwargs = {
            'appPath': self.appPath,
            'xrePath': self.xrePath,
            'testingModulesDir': self.testingModulesDir,
            'debuggerInfo': self.debuggerInfo,
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
            'failureManifest': failureManifest
        }

        if self.sequential:
            # Allow user to kill hung xpcshell subprocess with SIGINT
            # when we are only running tests sequentially.
            signal.signal(signal.SIGINT, markGotSIGINT)

        if self.debuggerInfo:
            # Force a sequential run
            self.sequential = True

            # If we have an interactive debugger, disable SIGINT entirely.
            if self.debuggerInfo["interactive"]:
                signal.signal(signal.SIGINT, lambda signum, frame: None)

        # create a queue of all tests that will run
        tests_queue = deque()
        # also a list for the tests that need to be run sequentially
        sequential_tests = []
        for test_object in self.alltests:
            name = test_object['path']
            if self.singleFile and not name.endswith(self.singleFile):
                continue

            if self.testPath and name.find(self.testPath) == -1:
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
            self.log.info("INFO | Running tests sequentially.")
        else:
            self.log.info("INFO | Using at most %d threads." % NUM_THREADS)

        # keep a set of NUM_THREADS running tests and start running the
        # tests in the queue at most NUM_THREADS at a time
        running_tests = set()
        keep_going = True
        exceptions = []
        tracebacks = []
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
                if not test.is_alive():
                    done_tests.add(test)
                    test.join()
                    # if the test had trouble, we will try running it again
                    # at the end of the run
                    if test.retry:
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
        # Clean up any slacker directories that might be lying around (Windows).
        # Some might fail because of windows taking too long to unlock them.
        # We don't do anything if this fails because the test slaves will have
        # their $TEMP dirs cleaned up on reboot anyway.
        for directory in self.cleanup_dir_list:
            try:
                shutil.rmtree(directory)
            except:
                self.log.info("INFO | %s could not be cleaned up." % directory)

        if exceptions:
            self.log.info("INFO | Following exceptions were raised:")
            for t in tracebacks:
                self.log.error(t)
            raise exceptions[0]

        if self.testCount == 0:
            self.log.error("TEST-UNEXPECTED-FAIL | runxpcshelltests.py | No tests run. Did you pass an invalid --test-path?")
            self.failCount = 1

        self.log.info("INFO | Result summary:")
        self.log.info("INFO | Passed: %d" % self.passCount)
        self.log.info("INFO | Failed: %d" % self.failCount)
        self.log.info("INFO | Todo: %d" % self.todoCount)
        self.log.info("INFO | Retried: %d" % len(self.try_again_list))

        if autolog:
            self.post_to_autolog(self.xunitResults, xunitName)

        if xunitFilename is not None:
            self.writeXunitResults(filename=xunitFilename, results=self.xunitResults,
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
        self.add_option("--autolog",
                        action="store_true", dest="autolog", default=False,
                        help="post to autolog")
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
        self.add_option("--xunit-file", dest="xunitFilename",
                        help="path to file where xUnit results will be written.")
        self.add_option("--xunit-suite-name", dest="xunitName",
                        help="name to record for this xUnit test suite. Many "
                             "tools expect Java class notation, e.g. "
                             "dom.basic.foo")
        self.add_option("--failure-manifest", dest="failureManifest",
                        action="store",
                        help="path to file where failure manifest will be written.")

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
