# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozunit import main

from common import BaseConfigureTest


class TestMozConfigure(BaseConfigureTest):
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
