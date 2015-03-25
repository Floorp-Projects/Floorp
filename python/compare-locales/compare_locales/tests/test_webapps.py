# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales import webapps


class TestFileComparison(unittest.TestCase):

    def mock_FileComparison(self, mock_listdir):
        class Target(webapps.FileComparison):
            def _listdir(self):
                return mock_listdir()
        return Target('.', 'en-US')

    def test_just_reference(self):
        def _listdir():
            return ['my_app.en-US.properties']
        filecomp = self.mock_FileComparison(_listdir)
        filecomp.files()
        self.assertEqual(filecomp.locales(), [])
        self.assertEqual(filecomp._reference.keys(), ['my_app'])
        file_ = filecomp._reference['my_app']
        self.assertEqual(file_.file, 'locales/my_app.en-US.properties')

    def test_just_locales(self):
        def _listdir():
            return ['my_app.ar.properties',
                    'my_app.sr-Latn.properties',
                    'my_app.sv-SE.properties',
                    'my_app.po_SI.properties']
        filecomp = self.mock_FileComparison(_listdir)
        filecomp.files()
        self.assertEqual(filecomp.locales(),
                         ['ar', 'sr-Latn', 'sv-SE'])
        self.assertEqual(filecomp._files['ar'].keys(), ['my_app'])
        file_ = filecomp._files['ar']['my_app']
        self.assertEqual(file_.file, 'locales/my_app.ar.properties')
