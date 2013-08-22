#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import sys, os, tempfile, shutil
from optparse import OptionParser
import mozprocess, mozinfo, mozlog, mozcrash, mozfile
from contextlib import contextmanager

log = mozlog.getLogger('cppunittests')

class CPPUnitTests(object):
    # Time (seconds) to wait for test process to complete
    TEST_PROC_TIMEOUT = 1200
    # Time (seconds) in which process will be killed if it produces no output.
    TEST_PROC_NO_OUTPUT_TIMEOUT = 300

    def run_one_test(self, prog, env, symbols_path=None):
        """
        Run a single C++ unit test program.

        Arguments:
        * prog: The path to the test program to run.
        * env: The environment to use for running the program.
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.

        Return True if the program exits with a zero status, False otherwise.
        """
        basename = os.path.basename(prog)
        log.info("Running test %s", basename)
        with mozfile.TemporaryDirectory() as tempdir:
            proc = mozprocess.ProcessHandler([prog],
                                             cwd=tempdir,
                                             env=env)
            #TODO: After bug 811320 is fixed, don't let .run() kill the process,
            # instead use a timeout in .wait() and then kill to get a stack.
            proc.run(timeout=CPPUnitTests.TEST_PROC_TIMEOUT,
                     outputTimeout=CPPUnitTests.TEST_PROC_NO_OUTPUT_TIMEOUT)
            proc.wait()
            if proc.timedOut:
                log.testFail("%s | timed out after %d seconds",
                             basename, CPPUnitTests.TEST_PROC_TIMEOUT)
                return False
            if mozcrash.check_for_crashes(tempdir, symbols_path,
                                          test_name=basename):
                log.testFail("%s | test crashed", basename)
                return False
            result = proc.proc.returncode == 0
            if not result:
                log.testFail("%s | test failed with return code %d",
                             basename, proc.proc.returncode)
            return result

    def build_core_environment(self, env = {}):
        """
        Add environment variables likely to be used across all platforms, including remote systems.
        """
        env["MOZILLA_FIVE_HOME"] = self.xre_path
        env["MOZ_XRE_DIR"] = self.xre_path
        #TODO: switch this to just abort once all C++ unit tests have
        # been fixed to enable crash reporting
        env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["MOZ_CRASHREPORTER"] = "1"
        return env

    def build_environment(self):
        """
        Create and return a dictionary of all the appropriate env variables and values.
        On a remote system, we overload this to set different values and are missing things like os.environ and PATH.
        """
        if not os.path.isdir(self.xre_path):
            raise Exception("xre_path does not exist: %s", self.xre_path)
        env = dict(os.environ)
        env = self.build_core_environment(env)
        pathvar = ""
        if mozinfo.os == "linux":
            pathvar = "LD_LIBRARY_PATH"
        elif mozinfo.os == "mac":
            pathvar = "DYLD_LIBRARY_PATH"
        elif mozinfo.os == "win":
            pathvar = "PATH"
        if pathvar:
            if pathvar in env:
                env[pathvar] = "%s%s%s" % (self.xre_path, os.pathsep, env[pathvar])
            else:
                env[pathvar] = self.xre_path
        return env

    def run_tests(self, programs, xre_path, symbols_path=None):
        """
        Run a set of C++ unit test programs.

        Arguments:
        * programs: An iterable containing paths to test programs.
        * xre_path: A path to a directory containing a XUL Runtime Environment.
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.

        Returns True if all test programs exited with a zero status, False
        otherwise.
        """
        self.xre_path = xre_path
        env = self.build_environment()
        result = True
        for prog in programs:
            single_result = self.run_one_test(prog, env, symbols_path)
            result = result and single_result
        return result

class CPPUnittestOptions(OptionParser):
    def __init__(self):
        OptionParser.__init__(self)
        self.add_option("--xre-path",
                        action = "store", type = "string", dest = "xre_path",
                        default = None,
                        help = "absolute path to directory containing XRE (probably xulrunner)")
        self.add_option("--symbols-path",
                        action = "store", type = "string", dest = "symbols_path",
                        default = None,
                        help = "absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols")

def extract_unittests_from_args(args):
    """Extract unittests from args, expanding directories as needed"""
    progs = []

    for p in args:
        if os.path.isdir(p):
            #filter out .py files packaged with the unit tests
            progs.extend([os.path.abspath(os.path.join(p, x)) for x in os.listdir(p) if not x.endswith('.py')])
        else:
            progs.append(os.path.abspath(p))

    return progs

def main():
    parser = CPPUnittestOptions()
    options, args = parser.parse_args()
    if not args:
        print >>sys.stderr, """Usage: %s <test binary> [<test binary>...]""" % sys.argv[0]
        sys.exit(1)
    if not options.xre_path:
        print >>sys.stderr, """Error: --xre-path is required"""
        sys.exit(1)
    progs = extract_unittests_from_args(args)
    options.xre_path = os.path.abspath(options.xre_path)
    tester = CPPUnitTests()
    try:
        result = tester.run_tests(progs, options.xre_path, options.symbols_path)
    except Exception, e:
        log.error(str(e))
        result = False
    sys.exit(0 if result else 1)

if __name__ == '__main__':
    main()

