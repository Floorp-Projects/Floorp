#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import os
from unittest import mock
import tempfile
import shutil
from contextlib import contextmanager
from pathlib import Path

from mach.registrar import Registrar

Registrar.categories = {"testing": []}
Registrar.commands_by_category = {"testing": set()}


from mozperftest.environment import MachEnvironment  # noqa
from mozperftest.mach_commands import Perftest, PerftestTests, ON_TRY  # noqa
from mozperftest import mach_commands  # noqa
from mozperftest.tests.support import EXAMPLE_TESTS_DIR  # noqa
from mozperftest.utils import temporary_env, silence  # noqa


ITERATION_HOOKS = Path(__file__).parent / "data" / "hooks_iteration.py"


class _TestMachEnvironment(MachEnvironment):
    def __init__(self, mach_cmd, flavor="desktop-browser", hooks=None, **kwargs):
        MachEnvironment.__init__(self, mach_cmd, flavor, hooks, **kwargs)
        self.runs = 0

    def run(self, metadata):
        self.runs += 1
        return metadata

    def __enter__(self):
        pass

    def __exit__(self, type, value, traceback):
        pass


@contextmanager
def _get_command(klass=Perftest):
    from mozbuild.base import MozbuildObject

    config = MozbuildObject.from_environment()

    class context:
        topdir = config.topobjdir
        cwd = os.getcwd()
        settings = {}
        log_manager = mock.Mock()
        state_dir = tempfile.mkdtemp()

    try:
        yield klass(context())
    finally:
        shutil.rmtree(context.state_dir)


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase.activate_virtualenv")
def test_command(mocked_func):
    with _get_command() as test, silence(test):
        test.run_perftest(tests=[EXAMPLE_TESTS_DIR], flavor="desktop-browser")
        # XXX add assertions


@mock.patch("mozperftest.MachEnvironment")
@mock.patch("mozperftest.mach_commands.MachCommandBase.activate_virtualenv")
def test_command_iterations(venv, env):
    kwargs = {
        "tests": [EXAMPLE_TESTS_DIR],
        "hooks": ITERATION_HOOKS,
        "flavor": "desktop-browser",
    }
    with _get_command() as test, silence(test):
        test.run_perftest(**kwargs)
    # the hook changes the iteration value to 5.
    # each iteration generates 5 calls, so we want to see 25
    assert len(env.mock_calls) == 25


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase.activate_virtualenv")
@mock.patch("tryselect.push.push_to_try")
def test_push_command(push_to_try, venv):
    with _get_command() as test, silence(test):
        test.run_perftest(
            tests=[EXAMPLE_TESTS_DIR],
            flavor="desktop-browser",
            push_to_try=True,
            try_platform="g5",
        )
        push_to_try.assert_called()
        # XXX add assertions


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase.activate_virtualenv")
def test_doc_flavor(mocked_func):
    with _get_command() as test, silence(test):
        test.run_perftest(tests=[EXAMPLE_TESTS_DIR], flavor="doc")


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase.activate_virtualenv")
@mock.patch("mozperftest.mach_commands.PerftestTests._run_python_script")
def test_test_runner(*mocked):
    # simulating on try to run the paths parser
    old = mach_commands.ON_TRY
    mach_commands.ON_TRY = True
    with _get_command(PerftestTests) as test, silence(test), temporary_env(
        MOZ_AUTOMATION="1"
    ):
        test.run_tests(tests=[EXAMPLE_TESTS_DIR])

    mach_commands.ON_TRY = old


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase.activate_virtualenv")
def test_run_python_script(*mocked):
    with _get_command(PerftestTests) as test, silence(test) as captured:
        test._run_python_script("lib2to3", *["--help"])

    stdout, stderr = captured
    stdout.seek(0)
    assert stdout.read() == "=> lib2to3 [OK]\n"


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase.activate_virtualenv")
def test_run_python_script_failed(*mocked):
    with _get_command(PerftestTests) as test, silence(test) as captured:
        test._run_python_script("nothing")

    stdout, stderr = captured
    stdout.seek(0)
    assert stdout.read().endswith("[FAILED]\n")


if __name__ == "__main__":
    mozunit.main()
