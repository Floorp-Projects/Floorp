# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

from buildconfig import topsrcdir
from common import BaseConfigureTest
from mozunit import main
from mozbuild.configure.options import InvalidOptionError


class TestToolkitMozConfigure(BaseConfigureTest):
    def test_necko_protocols(self):
        def get_value(arg):
            sandbox = self.get_sandbox({}, {}, [arg])
            return sandbox._value_for(sandbox['necko_protocols'])

        default_protocols = get_value('')
        self.assertNotEqual(default_protocols, ())

        # Backwards compatibility
        self.assertEqual(get_value('--enable-necko-protocols'),
                         default_protocols)

        self.assertEqual(get_value('--enable-necko-protocols=yes'),
                         default_protocols)

        self.assertEqual(get_value('--enable-necko-protocols=all'),
                         default_protocols)

        self.assertEqual(get_value('--enable-necko-protocols=default'),
                         default_protocols)

        self.assertEqual(get_value('--enable-necko-protocols='), ())

        self.assertEqual(get_value('--enable-necko-protocols=no'), ())

        self.assertEqual(get_value('--enable-necko-protocols=none'), ())

        self.assertEqual(get_value('--disable-necko-protocols'), ())

        self.assertEqual(get_value('--enable-necko-protocols=http'),
                         ('http',))

        self.assertEqual(get_value('--enable-necko-protocols=http,about'),
                         ('about', 'http'))

        self.assertEqual(get_value('--enable-necko-protocols=http,none'), ())

        self.assertEqual(get_value('--enable-necko-protocols=-http'), ())

        self.assertEqual(get_value('--enable-necko-protocols=none,http'),
                         ('http',))

        self.assertEqual(
            get_value('--enable-necko-protocols=all,-http,-about'),
            tuple(p for p in default_protocols if p not in ('http', 'about')))

        self.assertEqual(
            get_value('--enable-necko-protocols=default,-http,-about'),
            tuple(p for p in default_protocols if p not in ('http', 'about')))

    def test_developer_options(self):
        def get_value(args=[], environ={}):
            sandbox = self.get_sandbox({}, {}, args, environ)
            return sandbox._value_for(sandbox['developer_options'])

        self.assertEqual(get_value(), True)

        self.assertEqual(get_value(['--enable-release']), None)

        self.assertEqual(get_value(environ={'MOZILLA_OFFICIAL': 1}), None)

        self.assertEqual(get_value(['--enable-release'],
                         environ={'MOZILLA_OFFICIAL': 1}), None)

        with self.assertRaises(InvalidOptionError):
            get_value(['--disable-release'], environ={'MOZILLA_OFFICIAL': 1})

        self.assertEqual(get_value(environ={'MOZ_AUTOMATION': 1}), None)


if __name__ == '__main__':
    main()
