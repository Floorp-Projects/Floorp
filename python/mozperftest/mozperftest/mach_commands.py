# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import sys
from functools import partial
import subprocess

from mach.decorators import CommandProvider, Command, CommandArgument
from mozbuild.base import MachCommandBase, MachCommandConditions as conditions


_TRY_PLATFORMS = {"g5": "perftest-android-hw-g5", "p2": "perftest-android-hw-p2"}
ON_TRY = "MOZ_AUTOMATION" in os.environ


def get_perftest_parser():
    from mozperftest import PerftestArgumentParser

    return PerftestArgumentParser


@CommandProvider
class Perftest(MachCommandBase):
    @Command(
        "perftest",
        category="testing",
        conditions=[partial(conditions.is_buildapp_in, apps=["firefox", "android"])],
        description="Run any flavor of perftest",
        parser=get_perftest_parser,
    )
    def run_perftest(self, **kwargs):
        push_to_try = kwargs.pop("push_to_try", False)
        if push_to_try:
            from pathlib import Path

            sys.path.append(str(Path(self.topsrcdir, "tools", "tryselect")))

            from tryselect.push import push_to_try

            platform = kwargs.pop("try_platform")
            if platform not in _TRY_PLATFORMS:
                # we can extend platform support here: linux, win, macOs, pixel2
                # by adding more jobs in taskcluster/ci/perftest/kind.yml
                # then picking up the right one here
                raise NotImplementedError("%r not supported yet" % platform)

            perftest_parameters = {}
            parser = get_perftest_parser()()
            for name, value in kwargs.items():
                # ignore values that are set to default
                if parser.get_default(name) == value:
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

        # run locally
        MachCommandBase._activate_virtualenv(self)

        from mozperftest.runner import run_tests

        run_tests(mach_cmd=self, **kwargs)


@CommandProvider
class PerftestTests(MachCommandBase):
    def _run_python_script(self, module, *args, **kw):
        """Used to run the scripts in isolation.

        Coverage needs to run in isolation so it's not
        reimporting modules and produce wrong coverage info.
        """
        display = kw.pop("display", False)
        args = [self.virtualenv_manager.python_path, "-m", module] + list(args)
        sys.stdout.write("=> %s " % kw.pop("label", module))
        sys.stdout.flush()
        try:
            output = subprocess.check_output(args, stderr=subprocess.STDOUT)
            if display:
                print()
                for line in output.split(b"\n"):
                    print(line.decode("utf8"))
            sys.stdout.write("[OK]\n")
            sys.stdout.flush()
            return True
        except subprocess.CalledProcessError as e:
            for line in e.output.split(b"\n"):
                print(line.decode("utf8"))
            sys.stdout.write("[FAILED]\n")
            sys.stdout.flush()
            return False

    @Command(
        "perftest-test", category="testing", description="Run perftest tests",
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
        "-v", "--verbose", action="store_true", default=False, help="Verbose mode",
    )
    def run_tests(self, **kwargs):
        MachCommandBase._activate_virtualenv(self)

        from pathlib import Path
        from mozperftest.runner import _setup_path
        from mozperftest.utils import install_package, temporary_env

        skip_linters = kwargs.get("skip_linters", False)

        # include in sys.path all deps
        _setup_path()
        try:
            import coverage  # noqa
        except ImportError:
            pydeps = Path(self.topsrcdir, "third_party", "python")
            vendors = ["coverage"]
            if not ON_TRY:
                vendors.append("attrs")
            if skip_linters:
                pypis = []
            else:
                pypis = ["flake8"]

            # if we're not on try we want to install black
            if not ON_TRY and not skip_linters:
                pypis.append("black")

            # these are the deps we are getting from pypi
            for dep in pypis:
                install_package(self.virtualenv_manager, dep)

            # pip-installing dependencies that require compilation or special setup
            for dep in vendors:
                install_package(self.virtualenv_manager, str(Path(pydeps, dep)))

        here = Path(__file__).parent.resolve()
        if not ON_TRY and not skip_linters:
            # formatting the code with black
            assert self._run_python_script("black", str(here))

        # checking flake8 correctness
        if not (ON_TRY and sys.platform == "darwin") and not skip_linters:
            assert self._run_python_script("flake8", str(here))

        # running pytest with coverage
        # coverage is done in three steps:
        # 1/ coverage erase => erase any previous coverage data
        # 2/ coverage run pytest ... => run the tests and collect info
        # 3/ coverage report => generate the report
        tests_dir = Path(here, "tests").resolve()
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

        with temporary_env(COVERAGE_RCFILE=str(here / ".coveragerc")):
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
            assert self._run_python_script("coverage", *args, label="running tests")
            if run_coverage_check and not self._run_python_script(
                "coverage", "report", display=True
            ):
                raise ValueError("Coverage is too low!")
