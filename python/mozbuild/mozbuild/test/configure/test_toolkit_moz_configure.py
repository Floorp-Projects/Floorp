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
