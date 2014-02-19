#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import tempfile
import unittest

import mozfile
from mozversion import get_version


class SourcesTest(unittest.TestCase):
    """test getting version information from a sources xml"""

    application_ini = """[App]\nName = B2G\n"""
    sources_xml = """<?xml version="1.0" ?><manifest>
  <project path="build" revision="build_revision" />
  <project path="gaia" revision="gaia_revision" />
  <project path="gecko" revision="gecko_revision" />
</manifest>
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

    def test_sources(self):
        with open(os.path.join(self.tempdir, 'application.ini'), 'w') as f:
            f.writelines(self.application_ini)

        sources = os.path.join(self.tempdir, 'sources.xml')
        with open(sources, 'w') as f:
            f.writelines(self.sources_xml)

        os.chdir(self.tempdir)
        self._check_version(get_version(sources=sources))

    def test_sources_in_current_directory(self):
        with open(os.path.join(self.tempdir, 'application.ini'), 'w') as f:
            f.writelines(self.application_ini)

        with open(os.path.join(self.tempdir, 'sources.xml'), 'w') as f:
            f.writelines(self.sources_xml)

        os.chdir(self.tempdir)
        self._check_version(get_version())

    def test_invalid_sources_path(self):
        v = get_version(self.binary, os.path.join(self.tempdir, 'invalid'))
        self.assertEqual(v, {})

    def test_missing_sources_file(self):
        v = get_version(self.binary)
        self.assertEqual(v, {})

    def _check_version(self, version):
        self.assertEqual(version.get('build_changeset'), 'build_revision')
        self.assertEqual(version.get('gaia_changeset'), 'gaia_revision')
        self.assertEqual(version.get('gecko_changeset'), 'gecko_revision')
        self.assertIsNone(version.get('invalid_key'))


if __name__ == '__main__':
    unittest.main()
