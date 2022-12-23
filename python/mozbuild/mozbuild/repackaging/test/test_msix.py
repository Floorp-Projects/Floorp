# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from mozunit import main

from mozbuild.repackaging.msix import get_embedded_version


class TestMSIX(unittest.TestCase):
    def test_embedded_version(self):
        """Test embedded version extraction."""

        buildid = "YYYY0M0D0HMmSs"
        for input, output in [
            ("X.0a1", "X.YY0M.D0H.0"),
            ("X.YbZ", "X.Y.Z.0"),
            ("X.Yesr", "X.Y.0.0"),
            ("X.Y.Zesr", "X.Y.Z.0"),
            ("X.YrcZ", "X.Y.Z.0"),
            ("X.Y", "X.Y.0.0"),
            ("X.Y.Z", "X.Y.Z.0"),
        ]:
            version = get_embedded_version(input, buildid)
            self.assertEqual(version, output)
            # Some parts of the MSIX packaging ecosystem require the final digit
            # in the dotted quad to be 0.
            self.assertTrue(version.endswith(".0"))

        buildid = "YYYYMmDdHhMmSs"
        for input, output in [
            ("X.0a1", "X.YYMm.DdHh.0"),
        ]:
            version = get_embedded_version(input, buildid)
            self.assertEqual(version, output)
            # Some parts of the MSIX packaging ecosystem require the final digit
            # in the dotted quad to be 0.
            self.assertTrue(version.endswith(".0"))

        for input in [
            "X.Ya1",
            "X.0a2",
            "X.Y.ZbW",
            "X.Y.ZrcW",
        ]:
            with self.assertRaises(ValueError):
                get_embedded_version(input, buildid)


if __name__ == "__main__":
    main()
