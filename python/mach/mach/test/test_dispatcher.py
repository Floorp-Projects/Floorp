# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
from cStringIO import StringIO

from mach.base import CommandContext
from mach.registrar import Registrar
from mach.test.common import TestBase

from mozunit import main

here = os.path.abspath(os.path.dirname(__file__))


class TestDispatcher(TestBase):
    """Tests dispatch related code"""

    def get_parser(self, config=None):
        mach = self.get_mach('basic.py')

        for provider in Registrar.settings_providers:
            mach.settings.register_provider(provider)

        if config:
            if isinstance(config, basestring):
                config = StringIO(config)
            mach.settings.load_fps([config])

        context = CommandContext(settings=mach.settings)
        return mach.get_argument_parser(context)

    def test_command_aliases(self):
        config = """
[alias]
foo = cmd_foo
bar = cmd_bar
baz = cmd_bar --baz
cmd_bar = cmd_bar --baz
"""
        parser = self.get_parser(config=config)

        args = parser.parse_args(['foo'])
        self.assertEquals(args.command, 'cmd_foo')

        def assert_bar_baz(argv):
            args = parser.parse_args(argv)
            self.assertEquals(args.command, 'cmd_bar')
            self.assertTrue(args.command_args.baz)

        # The following should all result in |cmd_bar --baz|
        assert_bar_baz(['bar', '--baz'])
        assert_bar_baz(['baz'])
        assert_bar_baz(['cmd_bar'])


if __name__ == '__main__':
    main()
