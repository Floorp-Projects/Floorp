# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from pathlib import Path

import pytest
from buildconfig import topsrcdir
from mozunit import main

import mach

ALL_COMMANDS = [
    "cmd_bar",
    "cmd_foo",
    "cmd_foobar",
    "mach-commands",
    "mach-completion",
    "mach-debug-commands",
]


@pytest.fixture
def run_completion(run_mach):
    def inner(args=[]):
        mach_dir = Path(mach.__file__).parent
        providers = [
            Path("commands.py"),
            mach_dir / "commands" / "commandinfo.py",
        ]

        def context_handler(key):
            if key == "topdir":
                return topsrcdir

        args = ["mach-completion"] + args
        return run_mach(args, providers, context_handler=context_handler)

    return inner


def format(targets):
    return "\n".join(targets) + "\n"


def test_mach_completion(run_completion):
    result, stdout, stderr = run_completion()
    assert result == 0
    assert stdout == format(ALL_COMMANDS)

    result, stdout, stderr = run_completion(["cmd_f"])
    assert result == 0
    # While it seems like this should return only commands that have
    # 'cmd_f' as a prefix, the completion script will handle this case
    # properly.
    assert stdout == format(ALL_COMMANDS)

    result, stdout, stderr = run_completion(["cmd_foo"])
    assert result == 0
    assert stdout == format(["help", "--arg"])


@pytest.mark.parametrize("shell", ("bash", "fish", "zsh"))
def test_generate_mach_completion_script(run_completion, shell):
    rv, out, err = run_completion([shell])
    print(out)
    print(err, file=sys.stderr)
    assert rv == 0
    assert err == ""

    assert "cmd_foo" in out
    assert "arg" in out
    assert "cmd_foobar" in out
    assert "cmd_bar" in out


if __name__ == "__main__":
    main()
