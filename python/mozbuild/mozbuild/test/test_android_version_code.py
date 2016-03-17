# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozunit import main
import unittest

from mozbuild.android_version_code import (
    android_version_code_v0,
    android_version_code_v1,
)

class TestAndroidVersionCode(unittest.TestCase):
    def test_android_version_code_v0(self):
        # From https://treeherder.mozilla.org/#/jobs?repo=mozilla-central&revision=e25de9972a77.
        buildid = '20150708104620'
        arm_api9  = 2015070819
        arm_api11 = 2015070821
        x86_api9  = 2015070822
        self.assertEqual(android_version_code_v0(buildid, cpu_arch='armeabi', min_sdk=9, max_sdk=None), arm_api9)
        self.assertEqual(android_version_code_v0(buildid, cpu_arch='armeabi-v7a', min_sdk=11, max_sdk=None), arm_api11)
        self.assertEqual(android_version_code_v0(buildid, cpu_arch='x86', min_sdk=9, max_sdk=None), x86_api9)

    def test_android_version_code_v1(self):
        buildid = '20150825141628'
        arm_api15 = 0b01111000001000000001001001110001
        x86_api9  = 0b01111000001000000001001001110100
        self.assertEqual(android_version_code_v1(buildid, cpu_arch='armeabi-v7a', min_sdk=15, max_sdk=None), arm_api15)
        self.assertEqual(android_version_code_v1(buildid, cpu_arch='x86', min_sdk=9, max_sdk=None), x86_api9)

    def test_android_version_code_v1_underflow(self):
        '''Verify that it is an error to ask for v1 codes predating the cutoff.'''
        buildid = '201508010000' # Earliest possible.
        arm_api9 = 0b01111000001000000000000000000000
        self.assertEqual(android_version_code_v1(buildid, cpu_arch='armeabi', min_sdk=9, max_sdk=None), arm_api9)
        with self.assertRaises(ValueError) as cm:
            underflow = '201507310000' # Latest possible (valid) underflowing date.
            android_version_code_v1(underflow, cpu_arch='armeabi', min_sdk=9, max_sdk=None)
        self.assertTrue('underflow' in cm.exception.message)

    def test_android_version_code_v1_running_low(self):
        '''Verify there is an informative message if one asks for v1 codes that are close to overflow.'''
        with self.assertRaises(ValueError) as cm:
            overflow = '20290801000000'
            android_version_code_v1(overflow, cpu_arch='armeabi', min_sdk=9, max_sdk=None)
        self.assertTrue('Running out of low order bits' in cm.exception.message)

    def test_android_version_code_v1_overflow(self):
        '''Verify that it is an error to ask for v1 codes that actually does overflow.'''
        with self.assertRaises(ValueError) as cm:
            overflow = '20310801000000'
            android_version_code_v1(overflow, cpu_arch='armeabi', min_sdk=9, max_sdk=None)
        self.assertTrue('overflow' in cm.exception.message)

    def test_android_version_code_v0_relative_v1(self):
        '''Verify that the first v1 code is greater than the equivalent v0 code.'''
        buildid = '20150801000000'
        self.assertGreater(android_version_code_v1(buildid, cpu_arch='armeabi', min_sdk=9, max_sdk=None),
                           android_version_code_v0(buildid, cpu_arch='armeabi', min_sdk=9, max_sdk=None))


if __name__ == '__main__':
    main()
