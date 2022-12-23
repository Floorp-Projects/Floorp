# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from pathlib import Path
from unittest import mock
from unittest.mock import Mock, patch

import pytest
from mozbuild.base import MachCommandBase
from mozunit import main

import mach.decorators
import mach.registrar
from mach.base import MachError
from mach.decorators import Command, CommandArgument, SubCommand
from mach.requirements import MachEnvRequirements
from mach.site import CommandSiteManager, MozSiteMetadata, SitePackagesSource


@pytest.fixture
def registrar(monkeypatch):
    test_registrar = mach.registrar.MachRegistrar()
    test_registrar.register_category(
        "testing", "Mach unittest", "Testing for mach decorators"
    )
    monkeypatch.setattr(mach.decorators, "Registrar", test_registrar)
    return test_registrar


def test_register_command_with_argument(registrar):
    inner_function = Mock()
    context = Mock()
    context.cwd = "."

    @Command("cmd_foo", category="testing")
    @CommandArgument("--arg", default=None, help="Argument help.")
    def run_foo(command_context, arg):
        inner_function(arg)

    registrar.dispatch("cmd_foo", context, arg="argument")

    inner_function.assert_called_with("argument")


def test_register_command_with_metrics_path(registrar):
    context = Mock()
    context.cwd = "."

    metrics_path = "metrics/path"
    metrics_mock = Mock()
    context.telemetry.metrics.return_value = metrics_mock

    @Command("cmd_foo", category="testing", metrics_path=metrics_path)
    def run_foo(command_context):
        assert command_context.metrics == metrics_mock

    @SubCommand("cmd_foo", "sub_foo", metrics_path=metrics_path + "2")
    def run_subfoo(command_context):
        assert command_context.metrics == metrics_mock

    registrar.dispatch("cmd_foo", context)

    context.telemetry.metrics.assert_called_with(metrics_path)
    assert context.handler.metrics_path == metrics_path

    registrar.dispatch("cmd_foo", context, subcommand="sub_foo")
    assert context.handler.metrics_path == metrics_path + "2"


def test_register_command_sets_up_class_at_runtime(registrar):
    inner_function = Mock()

    context = Mock()
    context.cwd = "."

    # We test that the virtualenv is set up properly dynamically on
    # the instance that actually runs the command.
    @Command("cmd_foo", category="testing", virtualenv_name="env_foo")
    def run_foo(command_context):
        assert (
            Path(command_context.virtualenv_manager.virtualenv_root).name == "env_foo"
        )
        inner_function("foo")

    @Command("cmd_bar", category="testing", virtualenv_name="env_bar")
    def run_bar(command_context):
        assert (
            Path(command_context.virtualenv_manager.virtualenv_root).name == "env_bar"
        )
        inner_function("bar")

    def from_environment_patch(
        topsrcdir: str, state_dir: str, virtualenv_name, directory: str
    ):
        return CommandSiteManager(
            "",
            "",
            virtualenv_name,
            virtualenv_name,
            MozSiteMetadata(0, "mach", SitePackagesSource.VENV, "", ""),
            True,
            MachEnvRequirements(),
        )

    with mock.patch.object(
        CommandSiteManager, "from_environment", from_environment_patch
    ):
        with patch.object(MachCommandBase, "activate_virtualenv"):
            registrar.dispatch("cmd_foo", context)
            inner_function.assert_called_with("foo")
            registrar.dispatch("cmd_bar", context)
            inner_function.assert_called_with("bar")


def test_cannot_create_command_nonexisting_category(registrar):
    with pytest.raises(MachError):

        @Command("cmd_foo", category="bar")
        def run_foo(command_context):
            pass


def test_subcommand_requires_parent_to_exist(registrar):
    with pytest.raises(MachError):

        @SubCommand("sub_foo", "foo")
        def run_foo(command_context):
            pass


if __name__ == "__main__":
    main()
