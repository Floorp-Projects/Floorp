# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozunit import main
from mozpack import path as mozpath

from common import BaseConfigureTest


class TestMozConfigure(BaseConfigureTest):
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
            mozconfig='ac_add_options --enable-project=js')

        self.assertEquals('--enable-project=js MOZ_VTUNE=1',
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

    def test_nsis_version(self):
        this = self

        class FakeNSIS(object):
            def __init__(self, version):
                self.version = version

            def __call__(self, stdin, args):
                this.assertEquals(args, ('-version',))
                return 0, self.version, ''

        def check_nsis_version(version):
            sandbox = self.get_sandbox(
                {'/usr/bin/makensis': FakeNSIS(version)}, {}, [],
                {'PATH': '/usr/bin', 'MAKENSISU': '/usr/bin/makensis'})
            return sandbox._value_for(sandbox['nsis_version'])

        with self.assertRaises(SystemExit) as e:
            check_nsis_version('v2.5')

        with self.assertRaises(SystemExit) as e:
            check_nsis_version('v3.0a2')

        self.assertEquals(check_nsis_version('v3.0b1'), '3.0b1')
        self.assertEquals(check_nsis_version('v3.0b2'), '3.0b2')
        self.assertEquals(check_nsis_version('v3.0rc1'), '3.0rc1')
        self.assertEquals(check_nsis_version('v3.0'), '3.0')
        self.assertEquals(check_nsis_version('v3.0-2'), '3.0')
        self.assertEquals(check_nsis_version('v3.0.1'), '3.0')
        self.assertEquals(check_nsis_version('v3.1'), '3.1')


if __name__ == '__main__':
    main()
