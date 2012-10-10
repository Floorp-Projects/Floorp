#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import sys, optparse, os, tempfile, shutil
import mozprocess, mozinfo, mozlog, mozcrash
from contextlib import contextmanager

log = mozlog.getLogger('cppunittests')

@contextmanager
def TemporaryDirectory():
    tempdir = tempfile.mkdtemp()
    yield tempdir
    shutil.rmtree(tempdir)

def run_one_test(prog, env, symbols_path=None):
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
    with TemporaryDirectory() as tempdir:
        proc = mozprocess.ProcessHandler([prog],
                                         cwd=tempdir,
                                         env=env)
        proc.run()
        timeout = 300
        proc.processOutput(timeout=timeout)
        if proc.timedOut:
            log.testFail("%s | timed out after %d seconds",
                         basename, timeout)
            return False
        proc.waitForFinish(timeout=timeout)
        if proc.timedOut:
            log.testFail("%s | timed out after %d seconds",
                         basename, timeout)
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

def run_tests(programs, xre_path, symbols_path=None):
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
    if not os.path.isdir(xre_path):
        log.error("xre_path does not exist: %s", xre_path)
        return False
    #TODO: stick this in a module?
    env = dict(os.environ)
    pathvar = ""
    if mozinfo.os == "linux":
        pathvar = "LD_LIBRARY_PATH"
    elif mozinfo.os == "mac":
        pathvar = "DYLD_LIBRARY_PATH"
    elif mozinfo.os == "win":
        pathvar = "PATH"
    if pathvar:
        if pathvar in env:
            env[pathvar] = "%s%s%s" % (xre_path, os.pathsep, env[pathvar])
        else:
            env[pathvar] = xre_path
    env["MOZILLA_FIVE_HOME"] = xre_path
    env["MOZ_XRE_DIR"] = xre_path
    #TODO: switch this to just abort once all C++ unit tests have
    # been fixed to enable crash reporting
    env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
    env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
    env["MOZ_CRASHREPORTER"] = "1"
    result = True
    for prog in programs:
        single_result = run_one_test(prog, env, symbols_path)
        result = result and single_result
    return result

if __name__ == '__main__':
    parser = optparse.OptionParser()
    parser.add_option("--xre-path",
                      action = "store", type = "string", dest = "xre_path",
                      default = None,
                      help = "absolute path to directory containing XRE (probably xulrunner)")
    parser.add_option("--symbols-path",
                      action = "store", type = "string", dest = "symbols_path",
                      default = None,
                      help = "absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols")
    options, args = parser.parse_args()
    if not args:
        print >>sys.stderr, """Usage: %s <test binary> [<test binary>...]""" % sys.argv[0]
        sys.exit(1)
    if not options.xre_path:
        print >>sys.stderr, """Error: --xre-path is required"""
        sys.exit(1)
    progs = [os.path.abspath(p) for p in args]
    options.xre_path = os.path.abspath(options.xre_path)
    result = run_tests(progs, options.xre_path, options.symbols_path)
    sys.exit(0 if result else 1)
