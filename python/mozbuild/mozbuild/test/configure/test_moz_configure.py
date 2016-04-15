# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from StringIO import StringIO
import os
import tempfile
import textwrap
import unittest

from mozunit import main

from mozbuild.configure import ConfigureSandbox

from buildconfig import (
    topobjdir,
    topsrcdir,
)


class TestMozConfigure(unittest.TestCase):
    def setUp(self):
        self._cwd = os.getcwd()

    def tearDown(self):
        os.chdir(self._cwd)

    def get_value_for(self, key, args=[], environ={}, mozconfig=''):
        os.chdir(topobjdir)

        config = {}
        out = StringIO()

        fh, mozconfig_path = tempfile.mkstemp()
        os.write(fh, mozconfig)
        os.close(fh)

        try:
            environ = dict(environ,
                           OLD_CONFIGURE=os.path.join(topsrcdir, 'configure'),
                           MOZCONFIG=mozconfig_path)

            sandbox = ConfigureSandbox(config, environ, ['configure'] + args,
                                       out, out)
            sandbox.include_file(os.path.join(topsrcdir, 'moz.configure'))

            # Add a fake old-configure option
            sandbox.option_impl('--with-foo', nargs='*',
                                help='Help missing for old configure options')

            return sandbox._value_for(sandbox[key])
        finally:
            os.remove(mozconfig_path)

    def test_moz_configure_options(self):
        def get_value_for(args=[], environ={}, mozconfig=''):
            return self.get_value_for('all_configure_options', args, environ,
                                      mozconfig)

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
