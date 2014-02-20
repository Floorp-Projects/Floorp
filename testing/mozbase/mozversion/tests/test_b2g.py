#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import tempfile
import unittest
import zipfile

import mozfile
from mozversion import get_version


class SourcesTest(unittest.TestCase):
    """test getting version information from a sources xml"""

    def setUp(self):
        self.tempdir = tempfile.mkdtemp()

        self.binary = os.path.join(self.tempdir, 'binary')
        with open(self.binary, 'w') as f:
            f.write('foobar')

        with open(os.path.join(self.tempdir, 'application.ini'), 'w') as f:
            f.writelines("""[App]\nName = B2G\n""")

    def tearDown(self):
        mozfile.remove(self.tempdir)

    def _create_zip(self, revision, date='date'):
        zip_path = os.path.join(
            self.tempdir, 'gaia', 'profile', 'webapps',
            'settings.gaiamobile.org', 'application.zip')
        os.makedirs(os.path.dirname(zip_path))
        app_zip = zipfile.ZipFile(zip_path, 'w')
        app_zip.writestr('resources/gaia_commit.txt', revision + '\n' + date)
        app_zip.close()

    def test_gaia_commit(self):
        self._create_zip('a' * 40, 'date')
        v = get_version(self.binary)
        self.assertEqual(v.get('gaia_changeset'), 'a' * 40)
        self.assertEqual(v.get('gaia_date'), 'date')

    def test_invalid_gaia_commit(self):
        self._create_zip('a' * 41)
        v = get_version(self.binary)
        self.assertIsNone(v.get('gaia_changeset'))

    def test_missing_gaia_commit(self):
        v = get_version(self.binary)
        self.assertIsNone(v.get('gaia_changeset'))
        self.assertIsNone(v.get('gaia_date'))


if __name__ == '__main__':
    unittest.main()
