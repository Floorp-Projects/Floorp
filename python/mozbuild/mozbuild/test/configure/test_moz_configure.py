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

            result = sandbox._value_for(sandbox['all_configure_options'])
            shell = mozpath.abspath('/bin/sh')
            return result.replace('CONFIG_SHELL=%s ' % shell, '')

        self.assertEquals('--enable-application=browser',
                          get_value_for(['--enable-application=browser']))

        self.assertEquals('--enable-application=browser '
                          'MOZ_PROFILING=1',
                          get_value_for(['--enable-application=browser',
                                         'MOZ_PROFILING=1']))

        value = get_value_for(
            environ={'MOZ_PROFILING': '1'},
            mozconfig='ac_add_options --enable-project=js')

        self.assertEquals('--enable-project=js MOZ_PROFILING=1',
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


if __name__ == '__main__':
    main()
