#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import sys
import os
from optparse import OptionParser
import manifestparser
import mozprocess
import mozinfo
import mozcrash
import mozfile
import mozlog

SCRIPT_DIR = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))


class CPPUnitTests(object):
    # Time (seconds) to wait for test process to complete
    TEST_PROC_TIMEOUT = 900
    # Time (seconds) in which process will be killed if it produces no output.
    TEST_PROC_NO_OUTPUT_TIMEOUT = 300

    def run_one_test(self, prog, env, symbols_path=None, interactive=False,
                     timeout_factor=1):
        """
        Run a single C++ unit test program.

        Arguments:
        * prog: The path to the test program to run.
        * env: The environment to use for running the program.
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.
        * timeout_factor: An optional test-specific timeout multiplier.

        Return True if the program exits with a zero status, False otherwise.
        """
        basename = os.path.basename(prog)
        self.log.test_start(basename)
        with mozfile.TemporaryDirectory() as tempdir:
            if interactive:
                # For tests run locally, via mach, print output directly
                proc = mozprocess.ProcessHandler([prog],
                                                 cwd=tempdir,
                                                 env=env,
                                                 storeOutput=False)
            else:
                proc = mozprocess.ProcessHandler([prog],
                                                 cwd=tempdir,
                                                 env=env,
                                                 storeOutput=True,
                                                 processOutputLine=lambda _: None)
            # TODO: After bug 811320 is fixed, don't let .run() kill the process,
            # instead use a timeout in .wait() and then kill to get a stack.
            test_timeout = CPPUnitTests.TEST_PROC_TIMEOUT * timeout_factor
            proc.run(timeout=test_timeout,
                     outputTimeout=CPPUnitTests.TEST_PROC_NO_OUTPUT_TIMEOUT)
            proc.wait()
            if proc.output:
                output = "\n%s" % "\n".join(proc.output)
                self.log.process_output(proc.pid, output, command=[prog])
            if proc.timedOut:
                message = "timed out after %d seconds" % CPPUnitTests.TEST_PROC_TIMEOUT
                self.log.test_end(basename, status='TIMEOUT', expected='PASS',
                                  message=message)
                return False
            if mozcrash.check_for_crashes(tempdir, symbols_path,
                                          test_name=basename):
                self.log.test_end(basename, status='CRASH', expected='PASS')
                return False
            result = proc.proc.returncode == 0
            if not result:
                self.log.test_end(basename, status='FAIL', expected='PASS',
                                  message=("test failed with return code %d" %
                                           proc.proc.returncode))
            else:
                self.log.test_end(basename, status='PASS', expected='PASS')
            return result

    def build_core_environment(self, env={}):
        """
        Add environment variables likely to be used across all platforms, including remote systems.
        """
        env["MOZ_XRE_DIR"] = self.xre_path
        # TODO: switch this to just abort once all C++ unit tests have
        # been fixed to enable crash reporting
        env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["MOZ_CRASHREPORTER"] = "1"
        return env

    def build_environment(self):
        """
        Create and return a dictionary of all the appropriate env variables and values.
        On a remote system, we overload this to set different values and are missing things
        like os.environ and PATH.
        """
        if not os.path.isdir(self.xre_path):
            raise Exception("xre_path does not exist: %s", self.xre_path)
        env = dict(os.environ)
        env = self.build_core_environment(env)
        pathvar = ""
        libpath = self.xre_path
        if mozinfo.os == "linux":
            pathvar = "LD_LIBRARY_PATH"
        elif mozinfo.os == "mac":
            applibpath = os.path.join(os.path.dirname(libpath), 'MacOS')
            if os.path.exists(applibpath):
                # Set the library load path to Contents/MacOS if we're run from
                # the app bundle.
                libpath = applibpath
            pathvar = "DYLD_LIBRARY_PATH"
        elif mozinfo.os == "win":
            pathvar = "PATH"
        if pathvar:
            if pathvar in env:
                env[pathvar] = "%s%s%s" % (libpath, os.pathsep, env[pathvar])
            else:
                env[pathvar] = libpath

        if mozinfo.info["asan"]:
            # Use llvm-symbolizer for ASan if available/required
            llvmsym = os.path.join(
                self.xre_path,
                "llvm-symbolizer" + mozinfo.info["bin_suffix"].encode('ascii'))
            if os.path.isfile(llvmsym):
                env["ASAN_SYMBOLIZER_PATH"] = llvmsym
                self.log.info("ASan using symbolizer at %s" % llvmsym)
            else:
                self.log.info("Failed to find ASan symbolizer at %s" % llvmsym)

            # media/mtransport tests statically link in NSS, which
            # causes ODR violations. See bug 1215679.
            assert 'ASAN_OPTIONS' not in env
            env['ASAN_OPTIONS'] = 'detect_leaks=0:detect_odr_violation=0'

        return env

    def run_tests(self, programs, xre_path, symbols_path=None, interactive=False):
        """
        Run a set of C++ unit test programs.

        Arguments:
        * programs: An iterable containing (test path, test timeout factor) tuples
        * xre_path: A path to a directory containing a XUL Runtime Environment.
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.

        Returns True if all test programs exited with a zero status, False
        otherwise.
        """
        self.xre_path = xre_path
        self.log = mozlog.get_default_logger()
        self.log.suite_start(programs, name='cppunittest')
        env = self.build_environment()
        pass_count = 0
        fail_count = 0
        for prog in programs:
            test_path = prog[0]
            timeout_factor = prog[1]
            single_result = self.run_one_test(test_path, env, symbols_path,
                                              interactive, timeout_factor)
            if single_result:
                pass_count += 1
            else:
                fail_count += 1
        self.log.suite_end()

        # Mozharness-parseable summary formatting.
        self.log.info("Result summary:")
        self.log.info("cppunittests INFO | Passed: %d" % pass_count)
        self.log.info("cppunittests INFO | Failed: %d" % fail_count)
        return fail_count == 0


class CPPUnittestOptions(OptionParser):
    def __init__(self):
        OptionParser.__init__(self)
        self.add_option("--xre-path",
                        action="store", type="string", dest="xre_path",
                        default=None,
                        help="absolute path to directory containing XRE (probably xulrunner)")
        self.add_option("--symbols-path",
                        action="store", type="string", dest="symbols_path",
                        default=None,
                        help="absolute path to directory containing breakpad symbols, or "
                        "the URL of a zip file containing symbols")
        self.add_option("--manifest-path",
                        action="store", type="string", dest="manifest_path",
                        default=None,
                        help="path to test manifest, if different from the path to test binaries")


def extract_unittests_from_args(args, environ, manifest_path):
    """Extract unittests from args, expanding directories as needed"""
    mp = manifestparser.TestManifest(strict=True)
    tests = []
    binary_path = None

    if manifest_path:
        mp.read(manifest_path)
        binary_path = os.path.abspath(args[0])
    else:
        for p in args:
            if os.path.isdir(p):
                try:
                    mp.read(os.path.join(p, 'cppunittest.ini'))
                except IOError:
                    files = [os.path.abspath(os.path.join(p, x)) for x in os.listdir(p)]
                    tests.extend((f, 1) for f in files
                                 if os.access(f, os.R_OK | os.X_OK))
            else:
                tests.append((os.path.abspath(p), 1))

    # we skip the existence check here because not all tests are built
    # for all platforms (and it will fail on Windows anyway)
    active_tests = mp.active_tests(exists=False, disabled=False, **environ)
    suffix = '.exe' if mozinfo.isWin else ''
    if binary_path:
        tests.extend([
            (os.path.join(binary_path, test['relpath'] + suffix),
             int(test.get('requesttimeoutfactor', 1)))
            for test in active_tests])
    else:
        tests.extend([
            (test['path'] + suffix,
             int(test.get('requesttimeoutfactor', 1)))
            for test in active_tests
        ])

    # skip non-existing tests
    tests = [test for test in tests if os.path.isfile(test[0])]

    return tests


def update_mozinfo():
    """walk up directories to find mozinfo.json update the info"""
    path = SCRIPT_DIR
    dirs = set()
    while path != os.path.expanduser('~'):
        if path in dirs:
            break
        dirs.add(path)
        path = os.path.split(path)[0]
    mozinfo.find_and_update_from_json(*dirs)


def run_test_harness(options, args):
    update_mozinfo()
    progs = extract_unittests_from_args(args, mozinfo.info, options.manifest_path)
    options.xre_path = os.path.abspath(options.xre_path)
    tester = CPPUnitTests()
    result = tester.run_tests(progs, options.xre_path, options.symbols_path)

    return result


def main():
    parser = CPPUnittestOptions()
    mozlog.commandline.add_logging_group(parser)
    options, args = parser.parse_args()
    if not args:
        print >>sys.stderr, """Usage: %s <test binary> [<test binary>...]""" % sys.argv[0]
        sys.exit(1)
    if not options.xre_path:
        print >>sys.stderr, """Error: --xre-path is required"""
        sys.exit(1)
    if options.manifest_path and len(args) > 1:
        print >>sys.stderr, "Error: multiple arguments not supported with --test-manifest"
        sys.exit(1)
    log = mozlog.commandline.setup_logging("cppunittests", options,
                                           {"tbpl": sys.stdout})
    try:
        result = run_test_harness(options, args)
    except Exception as e:
        log.error(str(e))
        result = False

    sys.exit(0 if result else 1)


if __name__ == '__main__':
    main()
