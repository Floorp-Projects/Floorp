#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozfile
import unittest
import zipfile

import mozunit

from mozversion import get_version


class ApkTest(unittest.TestCase):
    """test getting version information from an android .apk"""

    application_changeset = 'a' * 40
    platform_changeset = 'b' * 40

    def create_apk_zipfiles(self, zfile):
        zfile.writestr('application.ini',
                       """[App]\nSourceStamp=%s\n""" % self.application_changeset)
        zfile.writestr('platform.ini',
                       """[Build]\nSourceStamp=%s\n""" % self.platform_changeset)
        zfile.writestr('AndroidManifest.xml', '')

    def test_basic(self):
        with mozfile.NamedTemporaryFile() as f:
            with zipfile.ZipFile(f.name, 'w') as z:
                self.create_apk_zipfiles(z)
            v = get_version(f.name)
            self.assertEqual(v.get('application_changeset'), self.application_changeset)
            self.assertEqual(v.get('platform_changeset'), self.platform_changeset)

    def test_with_package_name(self):
        with mozfile.NamedTemporaryFile() as f:
            with zipfile.ZipFile(f.name, 'w') as z:
                self.create_apk_zipfiles(z)
                z.writestr('package-name.txt', "org.mozilla.fennec")
            v = get_version(f.name)
            self.assertEqual(v.get('package_name'), "org.mozilla.fennec")

if __name__ == '__main__':
    mozunit.main()
