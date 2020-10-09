# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys

import pytest
from mozunit import main

from buildconfig import topsrcdir
import mach
from mach.test.common import TestBase

ALL_COMMANDS = [
    'cmd_bar',
    'cmd_foo',
    'cmd_foobar',
    'mach-commands',
    'mach-completion',
    'mach-debug-commands',
]


@pytest.fixture
def run_mach():

    tester = TestBase()

    def inner(args):
        mach_dir = os.path.dirname(mach.__file__)
        providers = [
            'commands.py',
            os.path.join(mach_dir, 'commands', 'commandinfo.py'),
        ]

        def context_handler(key):
            if key == 'topdir':
                return topsrcdir

        return tester._run_mach(args, providers, context_handler=context_handler)

    return inner


def format(targets):
    return "\n".join(targets) + "\n"


def test_mach_completion(run_mach):
    result, stdout, stderr = run_mach(['mach-completion'])
    assert result == 0
    assert stdout == format(ALL_COMMANDS)

    result, stdout, stderr = run_mach(['mach-completion', 'cmd_f'])
    assert result == 0
    # While it seems like this should return only commands that have
    # 'cmd_f' as a prefix, the completion script will handle this case
    # properly.
    assert stdout == format(ALL_COMMANDS)

    result, stdout, stderr = run_mach(['mach-completion', 'cmd_foo'])
    assert result == 0
    assert stdout == format(['help', '--arg'])


@pytest.mark.parametrize("shell", ("bash", "fish", "zsh"))
def test_generate_mach_completion_script(run_mach, shell):
    rv, out, err = run_mach(["mach-completion", shell])
    print(out)
    print(err, file=sys.stderr)
    assert rv == 0
    assert err == ""

    assert "cmd_foo" in out
    assert "arg" in out
    assert "cmd_foobar" in out
    assert "cmd_bar" in out


if __name__ == '__main__':
    main()
