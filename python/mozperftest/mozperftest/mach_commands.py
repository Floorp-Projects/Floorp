# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from functools import partial
import subprocess

from mach.decorators import CommandProvider, Command
from mozbuild.base import MachCommandBase, MachCommandConditions as conditions


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
        MachCommandBase._activate_virtualenv(self)

        from mozperftest.runner import run_tests

        run_tests(mach_cmd=self, **kwargs)


@CommandProvider
class PerftestTests(MachCommandBase):
    def _run_script(self, script, *args):
        """Used to run the scripts in isolation.

        Coverage needs to run in isolation so it's not
        reimporting modules and produce wrong coverage info.
        """
        script = str(script.resolve())
        args = [script] + list(args)
        try:
            return subprocess.check_call(args) == 0
        except subprocess.CalledProcessError:
            return False

    @Command(
        "perftest-test",
        category="testing",
        conditions=[partial(conditions.is_buildapp_in, apps=["firefox", "android"])],
        description="Run perftest tests",
    )
    def run_tests(self, **kwargs):
        MachCommandBase._activate_virtualenv(self)

        from mozperftest.utils import install_package

        for name in ("pytest", "coverage", "black"):
            install_package(self.virtualenv_manager, name)

        from pathlib import Path

        HERE = Path(__file__).parent.resolve()
        tests = HERE / "tests"
        venv_bin = Path(self.virtualenv_manager.virtualenv_root) / "bin"
        pytest = venv_bin / "pytest"
        coverage = venv_bin / "coverage"
        black = venv_bin / "black"

        # formatting the code with black
        assert self._run_script(black, str(HERE))

        # running pytest with coverage
        old_value = os.environ.get("COVERAGE_RCFILE")
        os.environ["COVERAGE_RCFILE"] = str(HERE / ".coveragerc")

        # coverage is done in three steps:
        # 1/ coverage erase => erase any previous coverage data
        # 2/ coverae run pytest ... => run the tests and collect info
        # 3/ coverage report => generate the report
        try:
            assert self._run_script(coverage, "erase")
            args = ["run", str(pytest.resolve()), "-xs", str(tests.resolve())]
            assert self._run_script(coverage, *args)
            if not self._run_script(coverage, "report"):
                raise ValueError("Coverage is too low!")
        finally:
            if old_value is not None:
                os.environ["COVERAGE_RCFILE"] = old_value
            else:
                del os.environ["COVERAGE_RCFILE"]
