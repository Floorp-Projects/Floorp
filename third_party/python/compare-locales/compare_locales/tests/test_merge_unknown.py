# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales.merge import merge_channels, MergeNotSupportedError


class TestMergeUnknown(unittest.TestCase):
    name = "foo.unknown"

    def test_not_supported_error(self):
        channels = (b"""
foo = Foo 1
""", b"""
foo = Foo 2
""")
        pattern = "Unsupported file format \(foo\.unknown\)\."
        with self.assertRaisesRegexp(MergeNotSupportedError, pattern):
            merge_channels(self.name, *channels)
