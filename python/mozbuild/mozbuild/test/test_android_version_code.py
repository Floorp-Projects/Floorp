# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozunit import main
import unittest

from mozbuild.android_version_code import android_version_code

class TestAndroidVersionCode(unittest.TestCase):
    def test_basic(self):
        # From https://treeherder.mozilla.org/#/jobs?repo=mozilla-central&revision=e25de9972a77.
        buildid = '20150708104620'
        arm_api9  = 2015070819
        arm_api11 = 2015070821
        x86_api9  = 2015070822
        self.assertEqual(android_version_code(buildid, cpu_arch='armeabi', min_sdk=9, max_sdk=None), arm_api9)
        self.assertEqual(android_version_code(buildid, cpu_arch='armeabi-v7a', min_sdk=11, max_sdk=None), arm_api11)
        self.assertEqual(android_version_code(buildid, cpu_arch='x86', min_sdk=9, max_sdk=None), x86_api9)


if __name__ == '__main__':
    main()
