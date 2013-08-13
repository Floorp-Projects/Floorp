#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import sys, os, tempfile, shutil
from optparse import OptionParser
import mozprocess, mozinfo, mozlog, mozcrash
from contextlib import contextmanager

log = mozlog.getLogger('gtest')

class GTests(object):
    # Time (seconds) to wait for test process to complete
    TEST_PROC_TIMEOUT = 1200
    # Time (seconds) in which process will be killed if it produces no output.
    TEST_PROC_NO_OUTPUT_TIMEOUT = 300

    def run_gtest(self, prog, xre_path, symbols_path=None):
        """
        Run a single C++ unit test program.

        Arguments:
        * prog: The path to the test program to run.
        * env: The environment to use for running the program.
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.

        Return True if the program exits with a zero status, False otherwise.
        """
        self.xre_path = xre_path
        env = self.build_environment()
        log.info("Running gtest")
        proc = mozprocess.ProcessHandler([prog, "-unittest"],
                                         cwd=os.getcwd(),
                                         env=env)
        #TODO: After bug 811320 is fixed, don't let .run() kill the process,
        # instead use a timeout in .wait() and then kill to get a stack.
        proc.run(timeout=GTests.TEST_PROC_TIMEOUT,
                 outputTimeout=GTests.TEST_PROC_NO_OUTPUT_TIMEOUT)
        proc.wait()
        if proc.timedOut:
            log.testFail("gtest | timed out after %d seconds", GTests.TEST_PROC_TIMEOUT)
            return False
        if mozcrash.check_for_crashes(os.getcwd(), symbols_path, test_name="gtest"):
            # mozcrash will output the log failure line for us.
            return False
        result = proc.proc.returncode == 0
        if not result:
            log.testFail("gtest | test failed with return code %d", proc.proc.returncode)
        return result

    def build_core_environment(self, env = {}):
        """
        Add environment variables likely to be used across all platforms, including remote systems.
        """
        env["MOZILLA_FIVE_HOME"] = self.xre_path
        env["MOZ_XRE_DIR"] = self.xre_path
        env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["MOZ_CRASHREPORTER"] = "1"
        env["MOZ_RUN_GTEST"] = "1"
        # Normally we run with GTest default output, override this to use the TBPL test format.
        env["MOZ_TBPL_PARSER"] = "1"
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

class gtestOptions(OptionParser):
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

def main():
    parser = gtestOptions()
    options, args = parser.parse_args()
    if not args:
        print >>sys.stderr, """Usage: %s <binary>""" % sys.argv[0]
        sys.exit(1)
    if not options.xre_path:
        print >>sys.stderr, """Error: --xre-path is required"""
        sys.exit(1)
    prog = os.path.abspath(args[0])
    options.xre_path = os.path.abspath(options.xre_path)
    tester = GTests()
    try:
        result = tester.run_gtest(prog, options.xre_path, options.symbols_path)
    except Exception, e:
        log.error(str(e))
        result = False
    sys.exit(0 if result else 1)

if __name__ == '__main__':
    main()

