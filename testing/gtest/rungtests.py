#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import sys

import mozcrash
import mozinfo
import mozlog
import mozprocess
from mozrunner.utils import get_stack_fixer_function

log = mozlog.unstructured.getLogger("gtest")


class GTests(object):
    # Time (seconds) to wait for test process to complete
    TEST_PROC_TIMEOUT = 2400
    # Time (seconds) in which process will be killed if it produces no output.
    TEST_PROC_NO_OUTPUT_TIMEOUT = 300

    def run_gtest(
        self,
        prog,
        xre_path,
        cwd,
        symbols_path=None,
        utility_path=None,
    ):
        """
        Run a single C++ unit test program.

        Arguments:
        * prog: The path to the test program to run.
        * env: The environment to use for running the program.
        * cwd: The directory to run tests from (support files will be found
               in this direcotry).
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.
        * utility_path: A path to a directory containing utility programs.
                        currently used to locate a stack fixer to provide
                        symbols symbols for assertion stacks.

        Return True if the program exits with a zero status, False otherwise.
        """
        self.xre_path = xre_path
        env = self.build_environment()
        log.info("Running gtest")

        if cwd and not os.path.isdir(cwd):
            os.makedirs(cwd)

        stack_fixer = None
        if utility_path:
            stack_fixer = get_stack_fixer_function(utility_path, symbols_path)

        GTests.run_gtest.timed_out = False

        def output_line_handler(proc, line):
            if stack_fixer:
                print(stack_fixer(line))
            else:
                print(line)

        def proc_timeout_handler(proc):
            GTests.run_gtest.timed_out = True
            log.testFail("gtest | timed out after %d seconds", GTests.TEST_PROC_TIMEOUT)
            mozcrash.kill_and_get_minidump(proc.pid, cwd, utility_path)

        def output_timeout_handler(proc):
            GTests.run_gtest.timed_out = True
            log.testFail(
                "gtest | timed out after %d seconds without output",
                GTests.TEST_PROC_NO_OUTPUT_TIMEOUT,
            )
            mozcrash.kill_and_get_minidump(proc.pid, cwd, utility_path)

        proc = mozprocess.run_and_wait(
            [prog, "-unittest", "--gtest_death_test_style=threadsafe"],
            cwd=cwd,
            env=env,
            output_line_handler=output_line_handler,
            timeout=GTests.TEST_PROC_TIMEOUT,
            timeout_handler=proc_timeout_handler,
            output_timeout=GTests.TEST_PROC_NO_OUTPUT_TIMEOUT,
            output_timeout_handler=output_timeout_handler,
        )

        log.info("gtest | process wait complete, returncode=%s" % proc.returncode)
        if mozcrash.check_for_crashes(cwd, symbols_path, test_name="gtest"):
            # mozcrash will output the log failure line for us.
            return False
        if GTests.run_gtest.timed_out:
            return False
        result = proc.returncode == 0
        if not result:
            log.testFail("gtest | test failed with return code %d", proc.returncode)
        return result

    def build_core_environment(self, env={}):
        """
        Add environment variables likely to be used across all platforms, including remote systems.
        """
        env["MOZ_XRE_DIR"] = self.xre_path
        env["MOZ_GMP_PATH"] = os.pathsep.join(
            os.path.join(self.xre_path, p, "1.0")
            for p in ("gmp-fake", "gmp-fakeopenh264")
        )
        env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["MOZ_CRASHREPORTER"] = "1"
        env["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"
        env["MOZ_RUN_GTEST"] = "1"
        # Normally we run with GTest default output, override this to use the TBPL test format.
        env["MOZ_TBPL_PARSER"] = "1"

        if not mozinfo.has_sandbox:
            # Bug 1082193 - This is horrible. Our linux build boxes run CentOS 6,
            # which is too old to support sandboxing. Disable sandbox for gtests
            # on machines which don't support sandboxing until they can be
            # upgraded, or gtests are run on test machines instead.
            env["MOZ_DISABLE_GMP_SANDBOX"] = "1"

        return env

    def build_environment(self):
        """
        Create and return a dictionary of all the appropriate env variables
        and values. On a remote system, we overload this to set different
        values and are missing things like os.environ and PATH.
        """
        if not os.path.isdir(self.xre_path):
            raise Exception("xre_path does not exist: %s", self.xre_path)
        env = dict(os.environ)
        env = self.build_core_environment(env)
        env["PERFHERDER_ALERTING_ENABLED"] = "1"
        pathvar = ""
        if mozinfo.os == "linux":
            pathvar = "LD_LIBRARY_PATH"
            # disable alerts for unstable tests (Bug 1369807)
            del env["PERFHERDER_ALERTING_ENABLED"]
        elif mozinfo.os == "mac":
            pathvar = "DYLD_LIBRARY_PATH"
        elif mozinfo.os == "win":
            pathvar = "PATH"
        if pathvar:
            if pathvar in env:
                env[pathvar] = "%s%s%s" % (self.xre_path, os.pathsep, env[pathvar])
            else:
                env[pathvar] = self.xre_path

        symbolizer_path = None
        if mozinfo.info["asan"]:
            symbolizer_path = "ASAN_SYMBOLIZER_PATH"
        elif mozinfo.info["tsan"]:
            symbolizer_path = "TSAN_SYMBOLIZER_PATH"

        if symbolizer_path is not None:
            # Use llvm-symbolizer for ASan/TSan if available/required
            if symbolizer_path in env and os.path.isfile(env[symbolizer_path]):
                llvmsym = env[symbolizer_path]
            else:
                llvmsym = os.path.join(
                    self.xre_path, "llvm-symbolizer" + mozinfo.info["bin_suffix"]
                )
            if os.path.isfile(llvmsym):
                env[symbolizer_path] = llvmsym
                log.info("Using LLVM symbolizer at %s", llvmsym)
            else:
                # This should be |testFail| instead of |info|. See bug 1050891.
                log.info("Failed to find LLVM symbolizer at %s", llvmsym)

        # webrender needs gfx.webrender.all=true, gtest doesn't use prefs
        env["MOZ_WEBRENDER"] = "1"
        env["MOZ_ACCELERATED"] = "1"

        return env


class gtestOptions(argparse.ArgumentParser):
    def __init__(self):
        super(gtestOptions, self).__init__()

        self.add_argument(
            "--cwd",
            dest="cwd",
            default=os.getcwd(),
            help="absolute path to directory from which " "to run the binary",
        )
        self.add_argument(
            "--xre-path",
            dest="xre_path",
            default=None,
            help="absolute path to directory containing XRE " "(probably xulrunner)",
        )
        self.add_argument(
            "--symbols-path",
            dest="symbols_path",
            default=None,
            help="absolute path to directory containing breakpad "
            "symbols, or the URL of a zip file containing "
            "symbols",
        )
        self.add_argument(
            "--utility-path",
            dest="utility_path",
            default=None,
            help="path to a directory containing utility program binaries",
        )
        self.add_argument("args", nargs=argparse.REMAINDER)


def update_mozinfo():
    """walk up directories to find mozinfo.json update the info"""
    path = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))
    dirs = set()
    while path != os.path.expanduser("~"):
        if path in dirs:
            break
        dirs.add(path)
        path = os.path.split(path)[0]
    mozinfo.find_and_update_from_json(*dirs)


def main():
    parser = gtestOptions()
    options = parser.parse_args()
    args = options.args
    if not args:
        print("Usage: %s <binary>" % sys.argv[0])
        sys.exit(1)
    if not options.xre_path:
        print("Error: --xre-path is required")
        sys.exit(1)
    if not options.utility_path:
        print("Warning: --utility-path is required to process assertion stacks")

    update_mozinfo()
    prog = os.path.abspath(args[0])
    options.xre_path = os.path.abspath(options.xre_path)
    tester = GTests()
    try:
        result = tester.run_gtest(
            prog,
            options.xre_path,
            options.cwd,
            symbols_path=options.symbols_path,
            utility_path=options.utility_path,
        )
    except Exception as e:
        log.error(str(e))
        result = False
    exit_code = 0 if result else 1
    log.info("rungtests.py exits with code %s" % exit_code)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
