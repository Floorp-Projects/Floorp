#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import mozunit
import os
import mock

from mach.registrar import Registrar

Registrar.categories = {"testing": []}
Registrar.commands_by_category = {"testing": set()}


from mozperftest.mach_commands import Perftest


def get_env(*args):
    class FakeModule:
        def __enter__(self):
            return self

        def __exit__(self, *args, **kw):
            pass

        def __call__(self, metadata):
            return metadata

    return FakeModule(), FakeModule(), FakeModule()


@mock.patch("mozperftest.get_env", new=get_env)
def test_command():
    from mozbuild.base import MozbuildObject

    config = MozbuildObject.from_environment()

    class context:
        topdir = config.topobjdir
        cwd = os.getcwd()
        settings = {}
        log_manager = {}

    test = Perftest(context())
    test.run_perftest()


if __name__ == "__main__":
    mozunit.main()
