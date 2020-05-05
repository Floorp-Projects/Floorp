#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import os
import mock
import tempfile
import shutil
from contextlib import contextmanager
import platform

from mach.registrar import Registrar

Registrar.categories = {"testing": []}
Registrar.commands_by_category = {"testing": set()}


from mozperftest.environment import MachEnvironment
from mozperftest.mach_commands import Perftest, PerftestTests
from mozperftest.tests.support import EXAMPLE_TESTS_DIR


class _TestMachEnvironment(MachEnvironment):
    def run(self, metadata):
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
        log_manager = {}
        state_dir = tempfile.mkdtemp()

    try:
        yield klass(context())
    finally:
        shutil.rmtree(context.state_dir)


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase._activate_virtualenv")
def test_command(mocked_func):
    with _get_command() as test:
        test.run_perftest(tests=[EXAMPLE_TESTS_DIR], flavor="script")
        # XXX add assertions


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase._activate_virtualenv")
def test_doc_flavor(mocked_func):
    with _get_command() as test:
        test.run_perftest(tests=[EXAMPLE_TESTS_DIR], flavor="doc")


@mock.patch("mozperftest.MachEnvironment", new=_TestMachEnvironment)
@mock.patch("mozperftest.mach_commands.MachCommandBase._activate_virtualenv")
@mock.patch("mozperftest.mach_commands.PerftestTests._run_script")
def test_test_runner(*mocked):
    if platform.system() == "Darwin":
        return
    with _get_command(PerftestTests) as test:
        test.run_tests(tests=[EXAMPLE_TESTS_DIR])


if __name__ == "__main__":
    mozunit.main()
