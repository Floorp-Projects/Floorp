# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals

from compare_locales.paths import File
from compare_locales.tests import BaseHelper


class TestProperties(BaseHelper):
    file = File('foo.properties', 'foo.properties')
    refContent = b'''some = value
'''

    def testGood(self):
        self._test(b'''some = localized''',
                   tuple())

    def testMissedEscape(self):
        self._test(br'''some = \u67ood escape, bad \escape''',
                   (('warning', 20, r'unknown escape sequence, \e',
                     'escape'),))


class TestPlurals(BaseHelper):
    file = File('foo.properties', 'foo.properties')
    refContent = b'''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file - Downloads;#1 files - #2
'''

    def testGood(self):
        self._test(b'''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file - Downloads;#1 files - #2;#1 filers
''',
                   tuple())

    def testNotUsed(self):
        self._test(b'''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file - Downloads;#1 files - Downloads;#1 filers
''',
                   (('warning', 0, 'not all variables used in l10n',
                     'plural'),))

    def testNotDefined(self):
        self._test(b'''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file - Downloads;#1 files - #2;#1 #3
''',
                   (('error', 0, 'unreplaced variables in l10n', 'plural'),))


class TestPluralForms(BaseHelper):
    file = File('foo.properties', 'foo.properties', locale='en-GB')
    refContent = b'''\
# LOCALIZATION NOTE (downloadsTitleFiles): Semi-colon list of plural forms.
# See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
# #1 number of files
# example: 111 files - Downloads
downloadsTitleFiles=#1 file;#1 files
'''

    def test_matching_forms(self):
        self._test(b'''\
downloadsTitleFiles=#1 fiiilee;#1 fiiilees
''',
                   tuple())

    def test_lacking_forms(self):
        self._test(b'''\
downloadsTitleFiles=#1 fiiilee
''',
                   (('warning', 0, 'expecting 2 plurals, found 1', 'plural'),))

    def test_excess_forms(self):
        self._test(b'''\
downloadsTitleFiles=#1 fiiilee;#1 fiiilees;#1 fiiilees
''',
                   (('warning', 0, 'expecting 2 plurals, found 3', 'plural'),))
