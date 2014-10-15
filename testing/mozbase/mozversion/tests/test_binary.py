#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import tempfile
import unittest

import mozfile

from mozversion import errors, get_version


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

    @unittest.skipIf(not os.environ.get('BROWSER_PATH'),
                     'No binary has been specified.')
    def test_real_binary(self):
        v = get_version(os.environ.get('BROWSER_PATH'))
        self.assertTrue(isinstance(v, dict))

    def test_binary(self):
        self._write_ini_files()

        self._check_version(get_version(self.binary))

    @unittest.skipIf(not hasattr(os, 'symlink'),
                     'os.symlink not supported on this platform')
    def test_symlinked_binary(self):
        self._write_ini_files()

        # create a symlink of the binary in another directory and check
        # version against this symlink
        tempdir = tempfile.mkdtemp()
        try:
            browser_link = os.path.join(tempdir,
                                        os.path.basename(self.binary))
            os.symlink(self.binary, browser_link)

            self._check_version(get_version(browser_link))
        finally:
            mozfile.remove(tempdir)

    def test_binary_in_current_path(self):
        self._write_ini_files()

        os.chdir(self.tempdir)
        self._check_version(get_version())

    def test_invalid_binary_path(self):
        self.assertRaises(IOError, get_version,
                          os.path.join(self.tempdir, 'invalid'))

    def test_without_ini_files(self):
        """With missing ini files an exception should be thrown"""
        self.assertRaises(errors.AppNotFoundError, get_version,
                          self.binary)

    def test_without_platform_file(self):
        """With a missing platform file no exception should be thrown"""
        self._write_ini_files(platform=False)

        v = get_version(self.binary)
        self.assertTrue(isinstance(v, dict))

    def test_with_exe(self):
        """Test that we can resolve .exe files"""
        self._write_ini_files()

        exe_name_unprefixed = self.binary + '1'
        exe_name = exe_name_unprefixed + '.exe'
        with open(exe_name, 'w') as f:
            f.write('foobar')
        self._check_version(get_version(exe_name_unprefixed))

    def _write_ini_files(self, application=True, platform=True):
        if application:
            with open(os.path.join(self.tempdir, 'application.ini'), 'w') as f:
                f.writelines(self.application_ini)
        if platform:
            with open(os.path.join(self.tempdir, 'platform.ini'), 'w') as f:
                f.writelines(self.platform_ini)

    def _check_version(self, version):
        self.assertEqual(version.get('application_name'), 'AppName')
        self.assertEqual(version.get('application_display_name'), 'AppCodeName')
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
