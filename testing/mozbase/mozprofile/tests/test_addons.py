#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import shutil
import tempfile
import unittest
import zipfile

import mozunit

from manifestparser import ManifestParser
import mozfile
import mozhttpd
import mozlog.unstructured as mozlog
import mozprofile

from addon_stubs import generate_addon, generate_manifest
from six.moves.urllib import error


here = os.path.dirname(os.path.abspath(__file__))


class TestAddonsManager(unittest.TestCase):
    """ Class to test mozprofile.addons.AddonManager """

    def setUp(self):
        self.logger = mozlog.getLogger('mozprofile.addons')
        self.logger.setLevel(mozlog.ERROR)

        self.profile = mozprofile.profile.Profile()
        self.am = self.profile.addon_manager

        self.profile_path = self.profile.profile
        self.tmpdir = tempfile.mkdtemp()
        self.addCleanup(mozfile.remove, self.tmpdir)

    def test_install_addons_multiple_same_source(self):
        # Generate installer stubs for all possible types of addons
        addon_xpi = generate_addon('test-addon-1@mozilla.org',
                                   path=self.tmpdir)
        addon_folder = generate_addon('test-addon-1@mozilla.org',
                                      path=self.tmpdir,
                                      xpi=False)

        # The same folder should not be installed twice
        self.am.install_addons([addon_folder, addon_folder])
        self.assertEqual(self.am.installed_addons, [addon_folder])
        self.am.clean()

        # The same XPI file should not be installed twice
        self.am.install_addons([addon_xpi, addon_xpi])
        self.assertEqual(self.am.installed_addons, [addon_xpi])
        self.am.clean()

        # Even if it is the same id the add-on should be installed twice, if
        # specified via XPI and folder
        self.am.install_addons([addon_folder, addon_xpi])
        self.assertEqual(len(self.am.installed_addons), 2)
        self.assertIn(addon_folder, self.am.installed_addons)
        self.assertIn(addon_xpi, self.am.installed_addons)
        self.am.clean()

    def test_download(self):
        server = mozhttpd.MozHttpd(docroot=os.path.join(here, 'addons'))
        server.start()

        # Download a valid add-on without a class instance to the general
        # tmp folder and clean-up
        try:
            addon = server.get_url() + 'empty.xpi'
            xpi_file = mozprofile.addons.AddonManager.download(addon)
            self.assertTrue(os.path.isfile(xpi_file))
            self.assertIn('test-empty@quality.mozilla.org.xpi',
                          os.path.basename(xpi_file))
            self.assertNotIn(self.tmpdir, os.path.dirname(xpi_file))
        finally:
            # Given that the file is stored outside of the created tmp dir
            # we have to ensure to explicitely remove it
            if os.path.isfile(xpi_file):
                os.remove(xpi_file)

        # Download an valid add-on to a special folder
        addon = server.get_url() + 'empty.xpi'
        xpi_file = self.am.download(addon, self.tmpdir)
        self.assertTrue(os.path.isfile(xpi_file))
        self.assertIn('test-empty@quality.mozilla.org.xpi',
                      os.path.basename(xpi_file))
        self.assertIn(self.tmpdir, os.path.dirname(xpi_file))
        self.assertEqual(self.am.downloaded_addons, [])
        os.remove(xpi_file)

        # Download an invalid add-on to a special folder
        addon = server.get_url() + 'invalid.xpi'
        self.assertRaises(mozprofile.addons.AddonFormatError,
                          self.am.download, addon, self.tmpdir)
        self.assertEqual(os.listdir(self.tmpdir), [])

        # Download from an invalid URL
        addon = server.get_url() + 'not_existent.xpi'
        self.assertRaises(error.HTTPError,
                          self.am.download, addon, self.tmpdir)
        self.assertEqual(os.listdir(self.tmpdir), [])

        # Download from an invalid URL
        addon = 'not_existent.xpi'
        self.assertRaises(ValueError,
                          self.am.download, addon, self.tmpdir)
        self.assertEqual(os.listdir(self.tmpdir), [])

        server.stop()

    def test_install_webextension_from_dir(self):
        addon = os.path.join(here, 'addons', 'apply-css.xpi')
        zipped = zipfile.ZipFile(addon)
        try:
            zipped.extractall(self.tmpdir)
        finally:
            zipped.close()
        self.am.install_from_path(self.tmpdir)
        self.assertEqual(len(self.am.installed_addons), 1)
        self.assertTrue(os.path.isdir(self.am.installed_addons[0]))

    def test_install_webextension(self):
        server = mozhttpd.MozHttpd(docroot=os.path.join(here, 'addons'))
        server.start()
        try:
            addon = server.get_url() + 'apply-css.xpi'
            self.am.install_from_path(addon)
        finally:
            server.stop()

        self.assertEqual(len(self.am.downloaded_addons), 1)
        self.assertTrue(os.path.isfile(self.am.downloaded_addons[0]))
        self.assertIn('test-webext@quality.mozilla.org.xpi',
                      os.path.basename(self.am.downloaded_addons[0]))

    def test_install_webextension_sans_id(self):
        server = mozhttpd.MozHttpd(docroot=os.path.join(here, 'addons'))
        server.start()
        try:
            addon = server.get_url() + 'apply-css-sans-id.xpi'
            self.am.install_from_path(addon)
        finally:
            server.stop()

        self.assertEqual(len(self.am.downloaded_addons), 1)
        self.assertTrue(os.path.isfile(self.am.downloaded_addons[0]))
        self.assertIn('temporary-addon.xpi',
                      os.path.basename(self.am.downloaded_addons[0]))

    def test_install_from_path_xpi(self):
        addons_to_install = []
        addons_installed = []

        # Generate installer stubs and install them
        for ext in ['test-addon-1@mozilla.org', 'test-addon-2@mozilla.org']:
            temp_addon = generate_addon(ext, path=self.tmpdir)
            addons_to_install.append(self.am.addon_details(temp_addon)['id'])
            self.am.install_from_path(temp_addon)

        # Generate a list of addons installed in the profile
        addons_installed = [str(x[:-len('.xpi')]) for x in os.listdir(os.path.join(
                            self.profile.profile, 'extensions', 'staged'))]
        self.assertEqual(addons_to_install.sort(), addons_installed.sort())

    def test_install_from_path_folder(self):
        # Generate installer stubs for all possible types of addons
        addons = []
        addons.append(generate_addon('test-addon-1@mozilla.org',
                                     path=self.tmpdir))
        addons.append(generate_addon('test-addon-2@mozilla.org',
                                     path=self.tmpdir,
                                     xpi=False))
        addons.append(generate_addon('test-addon-3@mozilla.org',
                                     path=self.tmpdir,
                                     name='addon-3'))
        addons.append(generate_addon('test-addon-4@mozilla.org',
                                     path=self.tmpdir,
                                     name='addon-4',
                                     xpi=False))
        addons.sort()

        self.am.install_from_path(self.tmpdir)

        self.assertEqual(self.am.installed_addons, addons)

    def test_install_from_path_unpack(self):
        # Generate installer stubs for all possible types of addons
        addon_xpi = generate_addon('test-addon-unpack@mozilla.org',
                                   path=self.tmpdir)
        addon_folder = generate_addon('test-addon-unpack@mozilla.org',
                                      path=self.tmpdir,
                                      xpi=False)
        addon_no_unpack = generate_addon('test-addon-1@mozilla.org',
                                         path=self.tmpdir)

        # Test unpack flag for add-on as XPI
        self.am.install_from_path(addon_xpi)
        self.assertEqual(self.am.installed_addons, [addon_xpi])
        self.am.clean()

        # Test unpack flag for add-on as folder
        self.am.install_from_path(addon_folder)
        self.assertEqual(self.am.installed_addons, [addon_folder])
        self.am.clean()

        # Test forcing unpack an add-on
        self.am.install_from_path(addon_no_unpack, unpack=True)
        self.assertEqual(self.am.installed_addons, [addon_no_unpack])
        self.am.clean()

    def test_install_from_path_url(self):
        server = mozhttpd.MozHttpd(docroot=os.path.join(here, 'addons'))
        server.start()

        addon = server.get_url() + 'empty.xpi'
        self.am.install_from_path(addon)

        server.stop()

        self.assertEqual(len(self.am.downloaded_addons), 1)
        self.assertTrue(os.path.isfile(self.am.downloaded_addons[0]))
        self.assertIn('test-empty@quality.mozilla.org.xpi',
                      os.path.basename(self.am.downloaded_addons[0]))

    def test_install_from_path_after_reset(self):
        # Installing the same add-on after a reset should not cause a failure
        addon = generate_addon('test-addon-1@mozilla.org',
                               path=self.tmpdir, xpi=False)

        # We cannot use self.am because profile.reset() creates a new instance
        self.profile.addon_manager.install_from_path(addon)

        self.profile.reset()

        self.profile.addon_manager.install_from_path(addon)
        self.assertEqual(self.profile.addon_manager.installed_addons, [addon])

    def test_install_from_path_backup(self):
        staged_path = os.path.join(self.profile_path, 'extensions', 'staged')

        # Generate installer stubs for all possible types of addons
        addon_xpi = generate_addon('test-addon-1@mozilla.org',
                                   path=self.tmpdir)
        addon_folder = generate_addon('test-addon-1@mozilla.org',
                                      path=self.tmpdir,
                                      xpi=False)
        addon_name = generate_addon('test-addon-1@mozilla.org',
                                    path=self.tmpdir,
                                    name='test-addon-1-dupe@mozilla.org')

        # Test backup of xpi files
        self.am.install_from_path(addon_xpi)
        self.assertIsNone(self.am.backup_dir)

        self.am.install_from_path(addon_xpi)
        self.assertIsNotNone(self.am.backup_dir)
        self.assertEqual(os.listdir(self.am.backup_dir),
                         ['test-addon-1@mozilla.org.xpi'])

        self.am.clean()
        self.assertEqual(os.listdir(staged_path),
                         ['test-addon-1@mozilla.org.xpi'])
        self.am.clean()

        # Test backup of folders
        self.am.install_from_path(addon_folder)
        self.assertIsNone(self.am.backup_dir)

        self.am.install_from_path(addon_folder)
        self.assertIsNotNone(self.am.backup_dir)
        self.assertEqual(os.listdir(self.am.backup_dir),
                         ['test-addon-1@mozilla.org'])

        self.am.clean()
        self.assertEqual(os.listdir(staged_path),
                         ['test-addon-1@mozilla.org'])
        self.am.clean()

        # Test backup of xpi files with another file name
        self.am.install_from_path(addon_name)
        self.assertIsNone(self.am.backup_dir)

        self.am.install_from_path(addon_xpi)
        self.assertIsNotNone(self.am.backup_dir)
        self.assertEqual(os.listdir(self.am.backup_dir),
                         ['test-addon-1@mozilla.org.xpi'])

        self.am.clean()
        self.assertEqual(os.listdir(staged_path),
                         ['test-addon-1@mozilla.org.xpi'])
        self.am.clean()

    def test_install_from_path_invalid_addons(self):
        # Generate installer stubs for all possible types of addons
        addons = []
        addons.append(generate_addon('test-addon-invalid-no-manifest@mozilla.org',
                                     path=self.tmpdir,
                                     xpi=False))
        addons.append(generate_addon('test-addon-invalid-no-id@mozilla.org',
                                     path=self.tmpdir))

        self.am.install_from_path(self.tmpdir)

        self.assertEqual(self.am.installed_addons, [])

    @unittest.skip("Feature not implemented as part of AddonManger")
    def test_install_from_path_error(self):
        """ Check install_from_path raises an error with an invalid addon"""

        temp_addon = generate_addon('test-addon-invalid-version@mozilla.org')
        # This should raise an error here
        self.am.install_from_path(temp_addon)

    def test_install_from_manifest(self):
        temp_manifest = generate_manifest(['test-addon-1@mozilla.org',
                                           'test-addon-2@mozilla.org'])
        m = ManifestParser()
        m.read(temp_manifest)
        addons = m.get()

        # Obtain details of addons to install from the manifest
        addons_to_install = [self.am.addon_details(x['path']).get('id') for x in addons]

        self.am.install_from_manifest(temp_manifest)
        # Generate a list of addons installed in the profile
        addons_installed = [str(x[:-len('.xpi')]) for x in os.listdir(os.path.join(
                            self.profile.profile, 'extensions', 'staged'))]
        self.assertEqual(addons_installed.sort(), addons_to_install.sort())

        # Cleanup the temporary addon and manifest directories
        mozfile.rmtree(os.path.dirname(temp_manifest))

    def test_addon_details(self):
        # Generate installer stubs for a valid and invalid add-on manifest
        valid_addon = generate_addon('test-addon-1@mozilla.org',
                                     path=self.tmpdir)
        invalid_addon = generate_addon('test-addon-invalid-not-wellformed@mozilla.org',
                                       path=self.tmpdir)

        # Check valid add-on
        details = self.am.addon_details(valid_addon)
        self.assertEqual(details['id'], 'test-addon-1@mozilla.org')
        self.assertEqual(details['name'], 'Test Add-on 1')
        self.assertEqual(details['unpack'], False)
        self.assertEqual(details['version'], '0.1')

        # Check invalid add-on
        self.assertRaises(mozprofile.addons.AddonFormatError,
                          self.am.addon_details, invalid_addon)

        # Check invalid path
        self.assertRaises(IOError,
                          self.am.addon_details, '')

        # Check invalid add-on format
        addon_path = os.path.join(os.path.join(here, 'files'), 'not_an_addon.txt')
        self.assertRaises(mozprofile.addons.AddonFormatError,
                          self.am.addon_details, addon_path)

    @unittest.skip("Bug 900154")
    def test_clean_addons(self):
        addon_one = generate_addon('test-addon-1@mozilla.org')
        addon_two = generate_addon('test-addon-2@mozilla.org')

        self.am.install_addons(addon_one)
        installed_addons = [str(x[:-len('.xpi')]) for x in os.listdir(os.path.join(
                            self.profile.profile, 'extensions', 'staged'))]

        # Create a new profile based on an existing profile
        # Install an extra addon in the new profile
        # Cleanup addons
        duplicate_profile = mozprofile.profile.Profile(profile=self.profile.profile,
                                                       addons=addon_two)
        duplicate_profile.addon_manager.clean()

        addons_after_cleanup = [str(x[:-len('.xpi')]) for x in os.listdir(os.path.join(
                                duplicate_profile.profile, 'extensions', 'staged'))]
        # New addons installed should be removed by clean_addons()
        self.assertEqual(installed_addons, addons_after_cleanup)

    def test_noclean(self):
        """test `restore=True/False` functionality"""

        server = mozhttpd.MozHttpd(docroot=os.path.join(here, 'addons'))
        server.start()

        profile = tempfile.mkdtemp()
        tmpdir = tempfile.mkdtemp()

        try:
            # empty initially
            self.assertFalse(bool(os.listdir(profile)))

            # make an addon
            addons = []
            addons.append(generate_addon('test-addon-1@mozilla.org',
                                         path=tmpdir))
            addons.append(server.get_url() + 'empty.xpi')

            # install it with a restore=True AddonManager
            am = mozprofile.addons.AddonManager(profile, restore=True)

            for addon in addons:
                am.install_from_path(addon)

            # now its there
            self.assertEqual(os.listdir(profile), ['extensions'])
            staging_folder = os.path.join(profile, 'extensions', 'staged')
            self.assertTrue(os.path.exists(staging_folder))
            self.assertEqual(len(os.listdir(staging_folder)), 2)

            # del addons; now its gone though the directory tree exists
            downloaded_addons = am.downloaded_addons
            del am

            self.assertEqual(os.listdir(profile), ['extensions'])
            self.assertTrue(os.path.exists(staging_folder))
            self.assertEqual(os.listdir(staging_folder), [])

            for addon in downloaded_addons:
                self.assertFalse(os.path.isfile(addon))

        finally:
            mozfile.rmtree(tmpdir)
            mozfile.rmtree(profile)

    def test_remove_addon(self):
        addons = []
        addons.append(generate_addon('test-addon-1@mozilla.org',
                                     path=self.tmpdir))
        addons.append(generate_addon('test-addon-2@mozilla.org',
                                     path=self.tmpdir))

        self.am.install_from_path(self.tmpdir)

        extensions_path = os.path.join(self.profile_path, 'extensions')
        staging_path = os.path.join(extensions_path, 'staged')

        # Fake a run by virtually installing one of the staged add-ons
        shutil.move(os.path.join(staging_path, 'test-addon-1@mozilla.org.xpi'),
                    extensions_path)

        for addon in self.am._addons:
            self.am.remove_addon(addon)

        self.assertEqual(os.listdir(staging_path), [])
        self.assertEqual(os.listdir(extensions_path), ['staged'])


if __name__ == '__main__':
    mozunit.main()
