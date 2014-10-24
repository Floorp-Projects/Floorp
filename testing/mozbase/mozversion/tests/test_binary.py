#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import tempfile
import shutil
import unittest

import mozfile

from mozversion import errors, get_version


class BinaryTest(unittest.TestCase):
    """test getting application version information from a binary path"""

    application_ini = """[App]
ID = AppID
Name = AppName
CodeName = AppCodeName
Version = AppVersion
BuildID = AppBuildID
SourceRepository = AppSourceRepo
SourceStamp = AppSourceStamp
Vendor = AppVendor
"""
    platform_ini = """[Build]
BuildID = PlatformBuildID
Milestone = PlatformMilestone
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

    def test_with_ini_files_on_osx(self):
        self._write_ini_files()

        platform = sys.platform
        sys.platform = 'darwin'
        try:
            # get_version is working with ini files next to the binary
            self._check_version(get_version(binary=self.binary))

            # or if they are in the Resources dir
            # in this case the binary must be in a Contents dir, next
            # to the Resources dir
            contents_dir = os.path.join(self.tempdir, 'Contents')
            os.mkdir(contents_dir)
            moved_binary = os.path.join(contents_dir,
                                        os.path.basename(self.binary))
            shutil.move(self.binary, moved_binary)

            resources_dir = os.path.join(self.tempdir, 'Resources')
            os.mkdir(resources_dir)
            for ini_file in ('application.ini', 'platform.ini'):
                shutil.move(os.path.join(self.tempdir, ini_file), resources_dir)

            self._check_version(get_version(binary=moved_binary))
        finally:
            sys.platform = platform

    def test_invalid_binary_path(self):
        self.assertRaises(IOError, get_version,
                          os.path.join(self.tempdir, 'invalid'))

    def test_without_ini_files(self):
        """With missing ini files an exception should be thrown"""
        self.assertRaises(errors.AppNotFoundError, get_version,
                          self.binary)

    def test_without_platform_ini_file(self):
        """With a missing platform.ini file an exception should be thrown"""
        self._write_ini_files(platform=False)
        self.assertRaises(errors.AppNotFoundError, get_version,
                          self.binary)

    def test_without_application_ini_file(self):
        """With a missing application.ini file an exception should be thrown"""
        self._write_ini_files(application=False)
        self.assertRaises(errors.AppNotFoundError, get_version,
                          self.binary)

    def test_with_exe(self):
        """Test that we can resolve .exe files"""
        self._write_ini_files()

        exe_name_unprefixed = self.binary + '1'
        exe_name = exe_name_unprefixed + '.exe'
        with open(exe_name, 'w') as f:
            f.write('foobar')
        self._check_version(get_version(exe_name_unprefixed))

    def test_not_found_with_binary_specified(self):
        self.assertRaises(errors.LocalAppNotFoundError, get_version, self.binary)

    def _write_ini_files(self, application=True, platform=True):
        if application:
            with open(os.path.join(self.tempdir, 'application.ini'), 'w') as f:
                f.writelines(self.application_ini)
        if platform:
            with open(os.path.join(self.tempdir, 'platform.ini'), 'w') as f:
                f.writelines(self.platform_ini)

    def _check_version(self, version):
        self.assertEqual(version.get('application_id'), 'AppID')
        self.assertEqual(version.get('application_name'), 'AppName')
        self.assertEqual(
            version.get('application_display_name'), 'AppCodeName')
        self.assertEqual(version.get('application_version'), 'AppVersion')
        self.assertEqual(version.get('application_buildid'), 'AppBuildID')
        self.assertEqual(
            version.get('application_repository'), 'AppSourceRepo')
        self.assertEqual(
            version.get('application_changeset'), 'AppSourceStamp')
        self.assertEqual(version.get('application_vendor'), 'AppVendor')
        self.assertIsNone(version.get('platform_name'))
        self.assertEqual(version.get('platform_buildid'), 'PlatformBuildID')
        self.assertEqual(
            version.get('platform_repository'), 'PlatformSourceRepo')
        self.assertEqual(
            version.get('platform_changeset'), 'PlatformSourceStamp')
        self.assertIsNone(version.get('invalid_key'))
        self.assertEqual(
            version.get('platform_version'), 'PlatformMilestone')


if __name__ == '__main__':
    unittest.main()
