# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import sys
from functools import partial
import subprocess
import shlex

from mach.decorators import CommandProvider, Command, CommandArgument
from mozbuild.base import MachCommandBase, MachCommandConditions as conditions


_TRY_PLATFORMS = {
    "g5": "perftest-android-hw-g5",
    "p2": "perftest-android-hw-p2",
    "linux": "perftest-linux-try",
    "mac": "perftest-macosx-try",
    "win": "perftest-windows-try",
}


HERE = os.path.dirname(__file__)


def get_perftest_parser():
    from mozperftest import PerftestArgumentParser

    return PerftestArgumentParser


@CommandProvider
class Perftest(MachCommandBase):
    def get_parser(self):
        return self.run_perftest._mach_command._parser

    @Command(
        "perftest",
        category="testing",
        conditions=[partial(conditions.is_buildapp_in, apps=["firefox", "android"])],
        description="Run any flavor of perftest",
        parser=get_perftest_parser,
    )
    def run_perftest(self, **kwargs):
        # original parser that brought us there
        original_parser = self.get_parser()

        from pathlib import Path

        # user selection with fuzzy UI
        from mozperftest.utils import ON_TRY
        from mozperftest.script import ScriptInfo, ScriptType, ParseError

        if not ON_TRY and kwargs.get("tests", []) == []:
            from moztest.resolve import TestResolver
            from mozperftest.fzf.fzf import select

            resolver = self._spawn(TestResolver)
            test_objects = list(resolver.resolve_tests(paths=None, flavor="perftest"))

            def full_path(selection):
                __, script_name, __, location = selection.split(" ")
                return str(
                    Path(
                        self.topsrcdir.rstrip(os.sep),
                        location.strip(os.sep),
                        script_name,
                    )
                )

            kwargs["tests"] = [full_path(s) for s in select(test_objects)]

            if kwargs["tests"] == []:
                print("\nNo selection. Bye!")
                return

        if len(kwargs["tests"]) > 1:
            print("\nSorry no support yet for multiple local perftest")
            return

        sel = "\n".join(kwargs["tests"])
        print("\nGood job! Best selection.\n%s" % sel)

        # if the script is xpcshell, we can force the flavor here
        # XXX on multi-selection,  what happens if we have seeveral flavors?
        try:
            script_info = ScriptInfo(kwargs["tests"][0])
        except ParseError as e:
            if e.exception is IsADirectoryError:
                script_info = None
            else:
                raise
        else:
            if script_info.script_type == ScriptType.xpcshell:
                kwargs["flavor"] = script_info.script_type.name
            else:
                # we set the value only if not provided (so "mobile-browser"
                # can be picked)
                if "flavor" not in kwargs:
                    kwargs["flavor"] = "desktop-browser"

        push_to_try = kwargs.pop("push_to_try", False)
        if push_to_try:
            sys.path.append(str(Path(self.topsrcdir, "tools", "tryselect")))

            from tryselect.push import push_to_try

            platform = kwargs.pop("try_platform")
            if platform not in _TRY_PLATFORMS:
                # we can extend platform support here: linux, win, macOs, pixel2
                # by adding more jobs in taskcluster/ci/perftest/kind.yml
                # then picking up the right one here
                raise NotImplementedError("%r not supported yet" % platform)

            perftest_parameters = {}
            args = script_info.update_args(**original_parser.get_user_args(kwargs))

            for name, value in args.items():
                # ignore values that are set to default
                if original_parser.get_default(name) == value:
                    continue
                perftest_parameters[name] = value

            parameters = {
                "try_task_config": {
                    "tasks": [_TRY_PLATFORMS[platform]],
                    "perftest-options": perftest_parameters,
                },
                "try_mode": "try_task_config",
            }

            task_config = {"parameters": parameters, "version": 2}
            push_to_try("perftest", "perftest", try_task_config=task_config)
            return

        from mozperftest.runner import run_tests

        run_tests(self, kwargs, original_parser.get_user_args(kwargs))

        print("\nFirefox. Fast For Good.\n")


@CommandProvider
class PerftestTests(MachCommandBase):
    def _run_script(self, cmd, *args, **kw):
        """Used to run a command in a subprocess."""
        display = kw.pop("display", False)
        verbose = kw.pop("verbose", False)
        if isinstance(cmd, str):
            cmd = shlex.split(cmd)
        try:
            joiner = shlex.join
        except AttributeError:
            # Python < 3.8
            joiner = subprocess.list2cmdline

        sys.stdout.write("=> %s " % kw.pop("label", joiner(cmd)))
        args = cmd + list(args)
        sys.stdout.flush()
        try:
            if verbose:
                sys.stdout.write("\nRunning %s\n" % " ".join(args))
                sys.stdout.flush()
            output = subprocess.check_output(args, stderr=subprocess.STDOUT)
            if display:
                sys.stdout.write("\n")
                for line in output.split(b"\n"):
                    sys.stdout.write(line.decode("utf8") + "\n")
            sys.stdout.write("[OK]\n")
            sys.stdout.flush()
            return True
        except subprocess.CalledProcessError as e:
            for line in e.output.split(b"\n"):
                sys.stdout.write(line.decode("utf8") + "\n")
            sys.stdout.write("[FAILED]\n")
            sys.stdout.flush()
            return False

    def _run_python_script(self, module, *args, **kw):
        """Used to run a Python script in isolation.

        Coverage needs to run in isolation so it's not
        reimporting modules and produce wrong coverage info.
        """
        cmd = [self.virtualenv_manager.python_path, "-m", module]
        if "label" not in kw:
            kw["label"] = module
        return self._run_script(cmd, *args, **kw)

    @Command(
        "perftest-test",
        category="testing",
        description="Run perftest tests",
    )
    @CommandArgument(
        "tests", default=None, nargs="*", help="Tests to run. By default will run all"
    )
    @CommandArgument(
        "-s",
        "--skip-linters",
        action="store_true",
        default=False,
        help="Skip flake8 and black",
    )
    @CommandArgument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="Verbose mode",
    )
    def run_tests(self, **kwargs):
        MachCommandBase.activate_virtualenv(self)

        from pathlib import Path
        from mozperftest.utils import temporary_env

        with temporary_env(
            COVERAGE_RCFILE=str(Path(HERE, ".coveragerc")), RUNNING_TESTS="YES"
        ):
            self._run_tests(**kwargs)

    def _run_tests(self, **kwargs):
        from pathlib import Path
        from mozperftest.runner import _setup_path
        from mozperftest.utils import install_package, ON_TRY

        skip_linters = kwargs.get("skip_linters", False)
        verbose = kwargs.get("verbose", False)

        # include in sys.path all deps
        _setup_path()
        try:
            import coverage  # noqa
        except ImportError:
            pydeps = Path(self.topsrcdir, "third_party", "python")
            vendors = ["coverage"]
            if not ON_TRY:
                vendors.append("attrs")

            # pip-installing dependencies that require compilation or special setup
            for dep in vendors:
                install_package(self.virtualenv_manager, str(Path(pydeps, dep)))

        if not ON_TRY and not skip_linters:
            cmd = "./mach lint "
            if verbose:
                cmd += " -v"
            cmd += " " + str(HERE)
            if not self._run_script(
                cmd, label="linters", display=verbose, verbose=verbose
            ):
                raise AssertionError("Please fix your code.")

        # running pytest with coverage
        # coverage is done in three steps:
        # 1/ coverage erase => erase any previous coverage data
        # 2/ coverage run pytest ... => run the tests and collect info
        # 3/ coverage report => generate the report
        tests_dir = Path(HERE, "tests").resolve()
        tests = kwargs.get("tests", [])
        if tests == []:
            tests = str(tests_dir)
            run_coverage_check = not skip_linters
        else:
            run_coverage_check = False

            def _get_test(test):
                if Path(test).exists():
                    return str(test)
                return str(tests_dir / test)

            tests = " ".join([_get_test(test) for test in tests])

        import pytest

        options = "-xs"
        if kwargs.get("verbose"):
            options += "v"

        if run_coverage_check:
            assert self._run_python_script(
                "coverage", "erase", label="remove old coverage data"
            )
        args = [
            "run",
            pytest.__file__,
            options,
            tests,
        ]
        assert self._run_python_script(
            "coverage", *args, label="running tests", verbose=verbose
        )
        if run_coverage_check and not self._run_python_script(
            "coverage", "report", display=True
        ):
            raise ValueError("Coverage is too low!")
