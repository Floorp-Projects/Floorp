# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

from StringIO import StringIO
from buildconfig import topsrcdir
from common import BaseConfigureTest
from mozunit import MockedOpen, main
from mozbuild.configure.options import InvalidOptionError
from mozbuild.configure.util import Version
from mozpack import path as mozpath


class TestToolkitMozConfigure(BaseConfigureTest):
    def test_moz_configure_options(self):
        def get_value_for(args=[], environ={}, mozconfig=''):
            sandbox = self.get_sandbox({}, {}, args, environ, mozconfig)

            # Add a fake old-configure option
            sandbox.option_impl('--with-foo', nargs='*',
                                help='Help missing for old configure options')

            # Remove all implied options, otherwise, getting
            # all_configure_options below triggers them, and that triggers
            # configure parts that aren't expected to run during this test.
            del sandbox._implied_options[:]
            result = sandbox._value_for(sandbox['all_configure_options'])
            shell = mozpath.abspath('/bin/sh')
            return result.replace('CONFIG_SHELL=%s ' % shell, '')

        self.assertEquals('--enable-application=browser',
                          get_value_for(['--enable-application=browser']))

        self.assertEquals('--enable-application=browser '
                          'MOZ_VTUNE=1',
                          get_value_for(['--enable-application=browser',
                                         'MOZ_VTUNE=1']))

        value = get_value_for(
            environ={'MOZ_VTUNE': '1'},
            mozconfig='ac_add_options --enable-application=browser')

        self.assertEquals('--enable-application=browser MOZ_VTUNE=1',
                          value)

        # --disable-js-shell is the default, so it's filtered out.
        self.assertEquals('--enable-application=browser',
                          get_value_for(['--enable-application=browser',
                                         '--disable-js-shell']))

        # Normally, --without-foo would be filtered out because that's the
        # default, but since it is a (fake) old-configure option, it always
        # appears.
        self.assertEquals('--enable-application=browser --without-foo',
                          get_value_for(['--enable-application=browser',
                                         '--without-foo']))
        self.assertEquals('--enable-application=browser --with-foo',
                          get_value_for(['--enable-application=browser',
                                         '--with-foo']))

        self.assertEquals("--enable-application=browser '--with-foo=foo bar'",
                          get_value_for(['--enable-application=browser',
                                         '--with-foo=foo bar']))

    def test_developer_options(self, milestone='42.0a1'):
        def get_value(args=[], environ={}):
            sandbox = self.get_sandbox({}, {}, args, environ)
            return sandbox._value_for(sandbox['developer_options'])

        milestone_path = os.path.join(topsrcdir, 'config', 'milestone.txt')
        with MockedOpen({milestone_path: milestone}):
            # developer options are enabled by default on "nightly" milestone
            # only
            self.assertEqual(get_value(), 'a' in milestone or None)

            self.assertEqual(get_value(['--enable-release']), None)

            self.assertEqual(get_value(environ={'MOZILLA_OFFICIAL': 1}), None)

            self.assertEqual(get_value(['--enable-release'],
                                       environ={'MOZILLA_OFFICIAL': 1}), None)

            with self.assertRaises(InvalidOptionError):
                get_value(['--disable-release'],
                          environ={'MOZILLA_OFFICIAL': 1})

            self.assertEqual(get_value(environ={'MOZ_AUTOMATION': 1}), None)

    def test_developer_options_release(self):
        self.test_developer_options('42.0')

    def test_valid_yasm_version(self):
        out = StringIO()
        sandbox = self.get_sandbox({}, {}, out=out)
        func = sandbox._depends[sandbox['valid_yasm_version']]._func

        # Missing yasm is not an error when nothing requires it.
        func(None, False, False, False)

        # Any version of yasm works when nothing requires it.
        func(Version('1.0'), False, False, False)

        # Any version of yasm works when something requires any version.
        func(Version('1.0'), True, False, False)
        func(Version('1.0'), True, True, False)
        func(Version('1.0'), False, True, False)

        # A version of yasm greater than any requirement works.
        func(Version('1.5'), Version('1.0'), True, False)
        func(Version('1.5'), True, Version('1.0'), False)
        func(Version('1.5'), Version('1.1'), Version('1.0'), False)

        out.truncate(0)
        with self.assertRaises(SystemExit):
            func(None, Version('1.0'), False, False)

        self.assertEqual(
            out.getvalue(),
            ('ERROR: Yasm is required to build with vpx, but you do not appear '
             'to have Yasm installed.\n'),
        )

        out.truncate(0)
        with self.assertRaises(SystemExit):
            func(None, Version('1.0'), Version('1.0'), False)

        self.assertEqual(
            out.getvalue(),
            ('ERROR: Yasm is required to build with jpeg and vpx, but you do not appear '
             'to have Yasm installed.\n'),
        )

        out.truncate(0)
        with self.assertRaises(SystemExit):
            func(None, Version('1.0'), Version('1.0'), Version('1.0'))

        self.assertEqual(
            out.getvalue(),
            ('ERROR: Yasm is required to build with jpeg, libav and vpx, but you do not appear '
             'to have Yasm installed.\n'),
        )

        out.truncate(0)
        with self.assertRaises(SystemExit):
            func(Version('1.0'), Version('1.1'), Version('1.0'), False)

        self.assertEqual(
            out.getvalue(),
            'ERROR: Yasm version 1.1 or greater is required to build with vpx.\n'
        )

        out.truncate(0)
        with self.assertRaises(SystemExit):
            func(Version('1.0'), True, Version('1.0.1'), False)

        self.assertEqual(
            out.getvalue(),
            'ERROR: Yasm version 1.0.1 or greater is required to build with jpeg.\n'
        )


if __name__ == '__main__':
    main()
