#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import os
import mock
import tempfile
import shutil

from mach.registrar import Registrar

Registrar.categories = {"testing": []}
Registrar.commands_by_category = {"testing": set()}


from mozperftest.environment import MachEnvironment
from mozperftest.mach_commands import Perftest


class TestMachEnvironment(MachEnvironment):
    def run(self, metadata):
        return metadata

    def __enter__(self):
        pass

    def __exit__(self, type, value, traceback):
        pass


@mock.patch("mozperftest.MachEnvironment", new=TestMachEnvironment)
def test_command():
    from mozbuild.base import MozbuildObject

    config = MozbuildObject.from_environment()

    class context:
        topdir = config.topobjdir
        cwd = os.getcwd()
        settings = {}
        log_manager = {}
        state_dir = tempfile.mkdtemp()

    try:
        test = Perftest(context())
        test.run_perftest()
    finally:
        shutil.rmtree(context.state_dir)


if __name__ == "__main__":
    mozunit.main()
