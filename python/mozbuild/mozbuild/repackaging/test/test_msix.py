# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import unittest

from mozbuild.repackaging.msix import get_embedded_version
from mozunit import main


class TestMSIX(unittest.TestCase):
    def test_embedded_version(self):
        """Test embedded version extraction."""

        buildid = "YYYY0M0D0HMmSs"
        for input, output in [
            ("X.YaZ", "X.YYYY.M0D.HMm"),
            ("X.YbZ", "X.Y.0.Z"),
            ("X.Y.ZbW", "X.Y.Z.W"),
            ("X.Yesr", "X.Y.0.0"),
            ("X.Y.Zesr", "X.Y.Z.0"),
            ("X.YrcZ", "X.Y.0.Z"),
            ("X.Y.ZrcW", "X.Y.Z.W"),
            ("X.Y", "X.Y.0.0"),
            ("X.Y.Z", "X.Y.Z.0"),
        ]:
            self.assertEquals(get_embedded_version(input, buildid), output)


if __name__ == "__main__":
    main()
