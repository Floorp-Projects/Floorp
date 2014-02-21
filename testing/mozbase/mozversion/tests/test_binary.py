#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import tempfile
import unittest

import mozfile
from mozversion import get_version


class BinaryTest(unittest.TestCase):
    """test getting application version information from a binary path"""

    application_ini = """[App]
Name = AppName
CodeName = AppCodeName
Version = AppVersion
BuildID = AppBuildID
SourceRepository = AppSourceRepo
SourceStamp = AppSourceStamp
"""
    platform_ini = """[Build]
BuildID = PlatformBuildID
SourceStamp = PlatformSourceStamp
SourceRepository = PlatformSourceRepo
"""

    def setUp(self):
        self.cwd = os.getcwd()
        self.tempdir = tempfile.mkdtemp()

        self.binary = os.path.join(self.tempdir, 'binary')
        with open(self.binary, 'w') as f:
            f.write('foobar')

    def tearDown(self):
        os.chdir(self.cwd)
        mozfile.remove(self.tempdir)

    def test_binary(self):
        with open(os.path.join(self.tempdir, 'application.ini'), 'w') as f:
            f.writelines(self.application_ini)

        with open(os.path.join(self.tempdir, 'platform.ini'), 'w') as f:
            f.writelines(self.platform_ini)

        self._check_version(get_version(self.binary))

    def test_binary_in_current_path(self):
        with open(os.path.join(self.tempdir, 'application.ini'), 'w') as f:
            f.writelines(self.application_ini)

        with open(os.path.join(self.tempdir, 'platform.ini'), 'w') as f:
            f.writelines(self.platform_ini)
        os.chdir(self.tempdir)
        self._check_version(get_version())

    def test_invalid_binary_path(self):
        self.assertRaises(IOError, get_version,
                          os.path.join(self.tempdir, 'invalid'))

    def test_missing_ini_files(self):
        v = get_version(self.binary)
        self.assertEqual(v, {})

    def _check_version(self, version):
        self.assertEqual(version.get('application_name'), 'AppName')
        self.assertEqual(version.get('application_code_name'), 'AppCodeName')
        self.assertEqual(version.get('application_version'), 'AppVersion')
        self.assertEqual(version.get('application_buildid'), 'AppBuildID')
        self.assertEqual(
            version.get('application_repository'), 'AppSourceRepo')
        self.assertEqual(
            version.get('application_changeset'), 'AppSourceStamp')
        self.assertIsNone(version.get('platform_name'))
        self.assertIsNone(version.get('platform_version'))
        self.assertEqual(version.get('platform_buildid'), 'PlatformBuildID')
        self.assertEqual(
            version.get('platform_repository'), 'PlatformSourceRepo')
        self.assertEqual(
            version.get('platform_changeset'), 'PlatformSourceStamp')
        self.assertIsNone(version.get('invalid_key'))


if __name__ == '__main__':
    unittest.main()
