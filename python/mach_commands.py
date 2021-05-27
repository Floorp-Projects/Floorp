# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import sys
import tempfile
from multiprocessing import cpu_count

import six

from concurrent.futures import (
    ThreadPoolExecutor,
    as_completed,
    thread,
)

import mozinfo
from mozfile import which
from manifestparser import TestManifest
from manifestparser import filters as mpf

from mozbuild.base import MachCommandBase

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)
from mach.util import UserError

here = os.path.abspath(os.path.dirname(__file__))


@CommandProvider
class MachCommands(MachCommandBase):
    @Command("python", category="devenv", description="Run Python.")
    @CommandArgument(
        "--no-virtualenv", action="store_true", help="Do not set up a virtualenv"
    )
    @CommandArgument(
        "--no-activate", action="store_true", help="Do not activate the virtualenv"
    )
    @CommandArgument(
        "--exec-file", default=None, help="Execute this Python file using `exec`"
    )
    @CommandArgument(
        "--ipython",
        action="store_true",
        default=False,
        help="Use ipython instead of the default Python REPL.",
    )
    @CommandArgument(
        "--requirements",
        default=None,
        help="Install this requirements file before running Python",
    )
    @CommandArgument("args", nargs=argparse.REMAINDER)
    def python(
        self,
        command_context,
        no_virtualenv,
        no_activate,
        exec_file,
        ipython,
        requirements,
        args,
    ):
        # Avoid logging the command
        self.log_manager.terminal_handler.setLevel(logging.CRITICAL)

        # Note: subprocess requires native strings in os.environ on Windows.
        append_env = {
            "PYTHONDONTWRITEBYTECODE": str("1"),
        }

        if requirements and no_virtualenv:
            raise UserError("Cannot pass both --requirements and --no-virtualenv.")

        if no_virtualenv:
            from mach_bootstrap import mach_sys_path

            python_path = sys.executable
            append_env["PYTHONPATH"] = os.pathsep.join(mach_sys_path(self.topsrcdir))
        else:
            self.virtualenv_manager.ensure()
            if not no_activate:
                self.virtualenv_manager.activate()
            python_path = self.virtualenv_manager.python_path
            if requirements:
                self.virtualenv_manager.install_pip_requirements(
                    requirements, require_hashes=False
                )

        if exec_file:
            exec(open(exec_file).read())
            return 0

        if ipython:
            bindir = os.path.dirname(python_path)
            python_path = which("ipython", path=bindir)
            if not python_path:
                if not no_virtualenv:
                    # Use `_run_pip` directly rather than `install_pip_package` to bypass
                    # `req.check_if_exists()` which may detect a system installed ipython.
                    self.virtualenv_manager._run_pip(["install", "ipython"])
                    python_path = which("ipython", path=bindir)

                if not python_path:
                    print("error: could not detect or install ipython")
                    return 1

        return self.run_process(
            [python_path] + args,
            pass_thru=True,  # Allow user to run Python interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            python_unbuffered=False,  # Leave input buffered.
            append_env=append_env,
        )

    @Command(
        "python-test",
        category="testing",
        virtualenv_name="python-test",
        description="Run Python unit tests with pytest.",
    )
    @CommandArgument(
        "-v", "--verbose", default=False, action="store_true", help="Verbose output."
    )
    @CommandArgument(
        "-j",
        "--jobs",
        default=None,
        type=int,
        help="Number of concurrent jobs to run. Default is the number of CPUs "
        "in the system.",
    )
    @CommandArgument(
        "-x",
        "--exitfirst",
        default=False,
        action="store_true",
        help="Runs all tests sequentially and breaks at the first failure.",
    )
    @CommandArgument(
        "--subsuite",
        default=None,
        help=(
            "Python subsuite to run. If not specified, all subsuites are run. "
            "Use the string `default` to only run tests without a subsuite."
        ),
    )
    @CommandArgument(
        "tests",
        nargs="*",
        metavar="TEST",
        help=(
            "Tests to run. Each test can be a single file or a directory. "
            "Default test resolution relies on PYTHON_UNITTEST_MANIFESTS."
        ),
    )
    @CommandArgument(
        "extra",
        nargs=argparse.REMAINDER,
        metavar="PYTEST ARGS",
        help=(
            "Arguments that aren't recognized by mach. These will be "
            "passed as it is to pytest"
        ),
    )
    def python_test(self, command_context, *args, **kwargs):
        try:
            tempdir = str(tempfile.mkdtemp(suffix="-python-test"))
            if six.PY2:
                os.environ[b"PYTHON_TEST_TMP"] = tempdir
            else:
                os.environ["PYTHON_TEST_TMP"] = tempdir
            return self.run_python_tests(*args, **kwargs)
        finally:
            import mozfile

            mozfile.remove(tempdir)

    def run_python_tests(
        self,
        tests=None,
        test_objects=None,
        subsuite=None,
        verbose=False,
        jobs=None,
        exitfirst=False,
        extra=None,
        **kwargs
    ):

        self.activate_virtualenv()
        if test_objects is None:
            from moztest.resolve import TestResolver

            resolver = self._spawn(TestResolver)
            # If we were given test paths, try to find tests matching them.
            test_objects = resolver.resolve_tests(paths=tests, flavor="python")
        else:
            # We've received test_objects from |mach test|. We need to ignore
            # the subsuite because python-tests don't use this key like other
            # harnesses do and |mach test| doesn't realize this.
            subsuite = None

        mp = TestManifest()
        mp.tests.extend(test_objects)

        filters = []
        if subsuite == "default":
            filters.append(mpf.subsuite(None))
        elif subsuite:
            filters.append(mpf.subsuite(subsuite))

        tests = mp.active_tests(
            filters=filters,
            disabled=False,
            python=self.virtualenv_manager.version_info()[0],
            **mozinfo.info
        )

        if not tests:
            submsg = "for subsuite '{}' ".format(subsuite) if subsuite else ""
            message = (
                "TEST-UNEXPECTED-FAIL | No tests collected "
                + "{}(Not in PYTHON_UNITTEST_MANIFESTS?)".format(submsg)
            )
            self.log(logging.WARN, "python-test", {}, message)
            return 1

        parallel = []
        sequential = []
        os.environ.setdefault("PYTEST_ADDOPTS", "")

        if extra:
            os.environ["PYTEST_ADDOPTS"] += " " + " ".join(extra)

        installed_requirements = set()
        for test in tests:
            if (
                test.get("requirements")
                and test["requirements"] not in installed_requirements
            ):
                self.virtualenv_manager.install_pip_requirements(
                    test["requirements"],
                    quiet=True,
                )
                installed_requirements.add(test["requirements"])

        if exitfirst:
            sequential = tests
            os.environ["PYTEST_ADDOPTS"] += " -x"
        else:
            for test in tests:
                if test.get("sequential"):
                    sequential.append(test)
                else:
                    parallel.append(test)

        self.jobs = jobs or cpu_count()
        self.terminate = False
        self.verbose = verbose

        return_code = 0

        def on_test_finished(result):
            output, ret, test_path = result

            for line in output:
                self.log(logging.INFO, "python-test", {"line": line.rstrip()}, "{line}")

            if ret and not return_code:
                self.log(
                    logging.ERROR,
                    "python-test",
                    {"test_path": test_path, "ret": ret},
                    "Setting retcode to {ret} from {test_path}",
                )
            return return_code or ret

        with ThreadPoolExecutor(max_workers=self.jobs) as executor:
            futures = [
                executor.submit(self._run_python_test, test) for test in parallel
            ]

            try:
                for future in as_completed(futures):
                    return_code = on_test_finished(future.result())
            except KeyboardInterrupt:
                # Hack to force stop currently running threads.
                # https://gist.github.com/clchiou/f2608cbe54403edb0b13
                executor._threads.clear()
                thread._threads_queues.clear()
                raise

        for test in sequential:
            return_code = on_test_finished(self._run_python_test(test))
            if return_code and exitfirst:
                break

        self.log(
            logging.INFO,
            "python-test",
            {"return_code": return_code},
            "Return code from mach python-test: {return_code}",
        )
        return return_code

    def _run_python_test(self, test):
        from mozprocess import ProcessHandler

        output = []

        def _log(line):
            # Buffer messages if more than one worker to avoid interleaving
            if self.jobs > 1:
                output.append(line)
            else:
                self.log(logging.INFO, "python-test", {"line": line.rstrip()}, "{line}")

        file_displayed_test = []  # used as boolean

        def _line_handler(line):
            line = six.ensure_str(line)
            if not file_displayed_test:
                output = (
                    "Ran" in line or "collected" in line or line.startswith("TEST-")
                )
                if output:
                    file_displayed_test.append(True)

            # Hack to make sure treeherder highlights pytest failures
            if "FAILED" in line.rsplit(" ", 1)[-1]:
                line = line.replace("FAILED", "TEST-UNEXPECTED-FAIL")

            _log(line)

        _log(test["path"])
        python = self.virtualenv_manager.python_path
        cmd = [python, test["path"]]
        env = os.environ.copy()
        if six.PY2:
            env[b"PYTHONDONTWRITEBYTECODE"] = b"1"
        else:
            env["PYTHONDONTWRITEBYTECODE"] = "1"

        # Homebrew on OS X will change Python's sys.executable to a custom value
        # which messes with mach's virtualenv handling code. Override Homebrew's
        # changes with the correct sys.executable value.
        if six.PY2:
            env[b"PYTHONEXECUTABLE"] = python.encode("utf-8")
        else:
            env["PYTHONEXECUTABLE"] = python

        proc = ProcessHandler(
            cmd, env=env, processOutputLine=_line_handler, storeOutput=False
        )
        proc.run()

        return_code = proc.wait()

        if not file_displayed_test:
            _log(
                "TEST-UNEXPECTED-FAIL | No test output (missing mozunit.main() "
                "call?): {}".format(test["path"])
            )

        if self.verbose:
            if return_code != 0:
                _log("Test failed: {}".format(test["path"]))
            else:
                _log("Test passed: {}".format(test["path"]))

        return output, return_code, test["path"]
