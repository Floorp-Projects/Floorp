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


class TestToolkitMozConfigure(BaseConfigureTest):
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
            'ERROR: Yasm is required to build with vpx, but you do not appear to have Yasm installed.\n'
        )

        out.truncate(0)
        with self.assertRaises(SystemExit):
            func(None, Version('1.0'), Version('1.0'), False)

        self.assertEqual(
            out.getvalue(),
            'ERROR: Yasm is required to build with jpeg and vpx, but you do not appear to have Yasm installed.\n'
        )

        out.truncate(0)
        with self.assertRaises(SystemExit):
            func(None, Version('1.0'), Version('1.0'), Version('1.0'))

        self.assertEqual(
            out.getvalue(),
            'ERROR: Yasm is required to build with jpeg, libav and vpx, but you do not appear to have Yasm installed.\n'
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
