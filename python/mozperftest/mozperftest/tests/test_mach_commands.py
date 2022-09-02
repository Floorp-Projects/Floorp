#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import os
import sys
from unittest import mock
import tempfile
import shutil
from contextlib import contextmanager
from pathlib import Path

import pytest
from mach.registrar import Registrar

Registrar.categories = {"testing": []}
Registrar.commands_by_category = {"testing": set()}

from mozbuild.base import MachCommandBase  # noqa

import mozperftest.mach_commands  # noqa
from mozperftest.environment import MachEnvironment  # noqa
from mozperftest.tests.support import EXAMPLE_TEST, ROOT, running_on_try  # noqa
from mozperftest.utils import temporary_env, silence  # noqa


ITERATION_HOOKS = Path(__file__).parent / "data" / "hooks_iteration.py"
STATE_HOOKS = Path(__file__).parent / "data" / "hooks_state.py"


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
def _get_command(command=mozperftest.mach_commands.run_perftest):
    from mozbuild.base import MozbuildObject
    from mozperftest.argparser import (
        PerftestArgumentParser,
    )

    config = MozbuildObject.from_environment()

    class context:
        topdir = config.topobjdir
        cwd = os.getcwd()
        settings = {}
        log_manager = mock.Mock()
        state_dir = tempfile.mkdtemp()

    # used to make arguments passed by the test as
    # being set by the user.
    def _run_perftest(func):
        def _run(command_context, **kwargs):
            parser.set_by_user = list(kwargs.keys())
            return func(command_context, **kwargs)

        return _run

    try:
        command_context = MachCommandBase(context())

        if command == mozperftest.mach_commands.run_perftest:
            parser = PerftestArgumentParser()
            command = _run_perftest(command)

        with mock.patch("mozperftest.mach_commands.get_parser", new=lambda: parser):
            yield command, command_context
    finally:
        shutil.rmtree(context.state_dir)


@contextmanager
def _get_tools_command(tool="side-by-side"):
    from mozbuild.base import MozbuildObject

    config = MozbuildObject.from_environment()

    class context:
        topdir = config.topobjdir
        cwd = os.getcwd()
        settings = {}
        log_manager = mock.Mock()
        state_dir = tempfile.mkdtemp()

    # used to make arguments passed by the test as
    # being set by the user.
    def _run_tool(func):
        def _run(command_context, **kwargs):
            parser.set_by_user = list(kwargs.keys())
            return func(command_context, **kwargs)

        return _run

    try:
        command_context = MachCommandBase(context())

        command = _run_tool(mozperftest.mach_commands.run_side_by_side)
        parser = mozperftest.mach_commands.get_perftest_tools_parser(tool)

        with mock.patch(
            "mozperftest.mach_commands.get_perftest_tools_parser", new=lambda: parser
        ):
            yield command, command_context
    finally:
        shutil.rmtree(context.state_dir)


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
def test_command(mocked_func):
    with _get_command() as (cmd, command_context), silence(command_context):
        cmd(command_context, tests=[EXAMPLE_TEST], flavor="desktop-browser")


@mock.patch("mozperftest.MachEnvironment")
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
def test_command_iterations(venv, env):
    kwargs = {
        "tests": [EXAMPLE_TEST],
        "hooks": ITERATION_HOOKS,
        "flavor": "desktop-browser",
    }
    with _get_command() as (cmd, command_context), silence(command_context):
        cmd(command_context, **kwargs)
        # the hook changes the iteration value to 5.
        # each iteration generates 5 calls, so we want to see 25
        assert len(env.mock_calls) == 25


@mock.patch("mozperftest.MachEnvironment")
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
def test_hooks_state(venv, env):
    kwargs = {
        "tests": [EXAMPLE_TEST],
        "hooks": STATE_HOOKS,
        "flavor": "desktop-browser",
    }
    with _get_command() as (cmd, command_context), silence(command_context):
        cmd(command_context, **kwargs)


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("tryselect.push.push_to_try")
def test_push_command(push_to_try, venv):
    with _get_command() as (cmd, command_context), silence(command_context):
        cmd(
            command_context,
            tests=[EXAMPLE_TEST],
            flavor="desktop-browser",
            push_to_try=True,
            try_platform="g5",
        )
        push_to_try.assert_called()
        # XXX add assertions


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("tryselect.push.push_to_try")
def test_push_command_unknown_platforms(push_to_try, venv):
    # full stop when a platform is unknown
    with _get_command() as (cmd, command_context), pytest.raises(NotImplementedError):
        cmd(
            command_context,
            tests=[EXAMPLE_TEST],
            flavor="desktop-browser",
            push_to_try=True,
            try_platform=["solaris", "linux", "mac"],
        )


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("tryselect.push.push_to_try")
def test_push_command_several_platforms(push_to_try, venv):
    with running_on_try(False), _get_command() as (
        cmd,
        command_context,
    ):  # , silence(command_context):
        cmd(
            command_context,
            tests=[EXAMPLE_TEST],
            flavor="desktop-browser",
            push_to_try=True,
            try_platform=["linux", "mac"],
        )
        push_to_try.assert_called()
        name, args, kwargs = push_to_try.mock_calls[0]
        params = kwargs["try_task_config"]["parameters"]["try_task_config"]
        assert "perftest-linux-try-browsertime" in params["tasks"]
        assert "perftest-macosx-try-browsertime" in params["tasks"]


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
def test_doc_flavor(mocked_func):
    with _get_command() as (cmd, command_context), silence(command_context):
        cmd(command_context, tests=[EXAMPLE_TEST], flavor="doc")


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("mozperftest.utils.run_script")
def test_test_runner(*mocked):
    from mozperftest.mach_commands import run_tests

    with running_on_try(False), _get_command(run_tests) as (cmd, command_context):
        cmd(command_context, tests=[EXAMPLE_TEST], verbose=True)


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("mozperftest.utils.run_python_script")
def test_test_runner_on_try(*mocked):
    from mozperftest.mach_commands import run_tests

    # simulating on try to run the paths parser
    with running_on_try(), _get_command(run_tests) as (cmd, command_context):
        cmd(command_context, tests=[EXAMPLE_TEST])


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("mozperftest.utils.run_script")
def test_test_runner_coverage(*mocked):
    from mozperftest.mach_commands import run_tests

    # simulating with coverage not installed
    with running_on_try(False), _get_command(run_tests) as (cmd, command_context):
        old = list(sys.meta_path)
        sys.meta_path = []
        try:
            cmd(command_context, tests=[EXAMPLE_TEST])
        finally:
            sys.meta_path = old


def fzf_selection(*args):
    try:
        full_path = args[-1][-1]["path"]
    except IndexError:
        return []

    path = Path(full_path.replace(str(ROOT), ""))
    return [f"[bt][sometag] {path.name} in {path.parent}"]


def resolve_tests(tests=None):
    if tests is None:
        tests = [{"path": str(EXAMPLE_TEST)}]

    def _resolve(*args, **kw):
        return tests

    return _resolve


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("mozperftest.fzf.fzf.select", new=fzf_selection)
@mock.patch("moztest.resolve.TestResolver.resolve_tests", new=resolve_tests())
def test_fzf_flavor(*mocked):
    with running_on_try(False), _get_command() as (
        cmd,
        command_context,
    ):  # , silence():
        cmd(command_context, flavor="desktop-browser")


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("mozperftest.fzf.fzf.select", new=fzf_selection)
@mock.patch("moztest.resolve.TestResolver.resolve_tests", new=resolve_tests([]))
def test_fzf_nothing_selected(*mocked):
    with running_on_try(False), _get_command() as (cmd, command_context), silence():
        cmd(command_context, flavor="desktop-browser")


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozbuild.base.MachCommandBase.activate_virtualenv")
@mock.patch("mozperftest.utils.run_python_script")
@mock.patch("mozperftest.utils.install_package")
def test_side_by_side(mock1, mock2, mock3, patched_mozperftest_tools):
    with mock.patch(
        "mozperftest.runner._create_artifacts_dir", return_value="fake_path"
    ) as _:
        with _get_tools_command() as (cmd, command_context), silence(command_context):
            cmd(command_context)
    patched_mozperftest_tools.run.assert_called()


if __name__ == "__main__":
    mozunit.main()
