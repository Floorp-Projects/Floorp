# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from mozunit import main

from mozbuild.configure.util import Version


class TestVersion(unittest.TestCase):
    def test_version_simple(self):
        v = Version('1')
        self.assertEqual(v, '1')
        self.assertLess(v, '2')
        self.assertGreater(v, '0.5')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 0)
        self.assertEqual(v.patch, 0)

    def test_version_more(self):
        v = Version('1.2.3b')
        self.assertLess(v, '2')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 2)
        self.assertEqual(v.patch, 3)

    def test_version_bad(self):
        # A version with a letter in the middle doesn't really make sense,
        # so everything after it should be ignored.
        v = Version('1.2b.3')
        self.assertLess(v, '2')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 2)
        self.assertEqual(v.patch, 0)

    def test_version_badder(self):
        v = Version('1b.2.3')
        self.assertLess(v, '2')
        self.assertEqual(v.major, 1)
        self.assertEqual(v.minor, 0)
        self.assertEqual(v.patch, 0)


if __name__ == '__main__':
    main()
