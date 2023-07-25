# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from io import StringIO
from pathlib import Path

import pytest
from mozunit import main
from six import string_types

from mach.base import CommandContext
from mach.registrar import Registrar


@pytest.mark.usefixtures("get_mach", "run_mach")
class TestDispatcher(unittest.TestCase):
    """Tests dispatch related code"""

    def get_parser(self, config=None):
        mach = self.get_mach(Path("basic.py"))

        for provider in Registrar.settings_providers:
            mach.settings.register_provider(provider)

        if config:
            if isinstance(config, string_types):
                config = StringIO(config)
            mach.settings.load_fps([config])

        context = CommandContext(cwd="", settings=mach.settings)
        from mach.main import get_argument_parser

        return get_argument_parser(context)

    def test_command_aliases(self):
        config = """
[alias]
foo = cmd_foo
bar = cmd_bar
baz = cmd_bar --baz
cmd_bar = cmd_bar --baz
"""
        parser = self.get_parser(config=config)

        args = parser.parse_args(["foo"])
        self.assertEqual(args.command, "cmd_foo")

        def assert_bar_baz(argv):
            args = parser.parse_args(argv)
            self.assertEqual(args.command, "cmd_bar")
            self.assertTrue(args.command_args.baz)

        # The following should all result in |cmd_bar --baz|
        assert_bar_baz(["bar", "--baz"])
        assert_bar_baz(["baz"])
        assert_bar_baz(["cmd_bar"])


if __name__ == "__main__":
    main()
