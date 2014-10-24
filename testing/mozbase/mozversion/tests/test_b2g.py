#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import tempfile
import unittest
import zipfile

import mozfile
from mozversion import get_version, errors


class SourcesTest(unittest.TestCase):
    """test getting version information from a sources xml"""

    def setUp(self):
        self.tempdir = tempfile.mkdtemp()

        self.binary = os.path.join(self.tempdir, 'binary')
        with open(self.binary, 'w') as f:
            f.write('foobar')

        with open(os.path.join(self.tempdir, 'application.ini'), 'w') as f:
            f.writelines("""[App]\nName = B2G\n""")

        with open(os.path.join(self.tempdir, 'platform.ini'), 'w') as f:
            f.write('[Build]\nBuildID = PlatformBuildID\n')

    def tearDown(self):
        mozfile.remove(self.tempdir)

    def _create_zip(self, revision=None, date=None):
        zip_path = os.path.join(
            self.tempdir, 'gaia', 'profile', 'webapps',
            'settings.gaiamobile.org', 'application.zip')
        os.makedirs(os.path.dirname(zip_path))
        app_zip = zipfile.ZipFile(zip_path, 'w')
        if revision or date:
            app_zip.writestr('resources/gaia_commit.txt',
                             '%s\n%s' % (revision, date))
        app_zip.close()

    def test_gaia_commit(self):
        revision, date = ('a' * 40, 'date')
        self._create_zip(revision, date)
        v = get_version(self.binary)
        self.assertEqual(v.get('gaia_changeset'), revision)
        self.assertEqual(v.get('gaia_date'), date)

    def test_invalid_gaia_commit(self):
        revision, date = ('a' * 41, 'date')
        self._create_zip(revision, date)
        v = get_version(self.binary)
        self.assertIsNone(v.get('gaia_changeset'))
        self.assertEqual(v.get('gaia_date'), date)

    def test_missing_zip_file(self):
        v = get_version(self.binary)
        self.assertIsNone(v.get('gaia_changeset'))
        self.assertIsNone(v.get('gaia_date'))

    def test_missing_gaia_commit(self):
        self._create_zip()
        v = get_version(self.binary)
        self.assertIsNone(v.get('gaia_changeset'))
        self.assertIsNone(v.get('gaia_date'))

    def test_b2g_fallback_when_no_binary(self):
        self.assertRaises(errors.RemoteAppNotFoundError, get_version)

if __name__ == '__main__':
    unittest.main()
