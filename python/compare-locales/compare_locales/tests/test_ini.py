# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales.tests import ParserTestMixin


mpl2 = '''\
; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this file,
; You can obtain one at http://mozilla.org/MPL/2.0/.
'''


class TestIniParser(ParserTestMixin, unittest.TestCase):

    filename = 'foo.ini'

    def testSimpleHeader(self):
        self._test('''; This file is in the UTF-8 encoding
[Strings]
TitleText=Some Title
''', (
            ('Comment', 'UTF-8 encoding'),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title'),))

    def testMPL2_Space_UTF(self):
        self._test(mpl2 + '''
; This file is in the UTF-8 encoding
[Strings]
TitleText=Some Title
''', (
            ('Comment', mpl2),
            ('Comment', 'UTF-8'),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title'),))

    def testMPL2_Space(self):
        self._test(mpl2 + '''
[Strings]
TitleText=Some Title
''', (
            ('Comment', mpl2),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title'),))

    def testMPL2_MultiSpace(self):
        self._test(mpl2 + '''\

; more comments

[Strings]
TitleText=Some Title
''', (
            ('Comment', mpl2),
            ('Comment', 'more comments'),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title'),))

    def testMPL2_JunkBeforeCategory(self):
        self._test(mpl2 + '''\
Junk
[Strings]
TitleText=Some Title
''', (
            ('Comment', mpl2),
            ('Junk', 'Junk'),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title')))

    def test_TrailingComment(self):
        self._test(mpl2 + '''
[Strings]
TitleText=Some Title
;Stray trailing comment
''', (
            ('Comment', mpl2),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title'),
            ('Comment', 'Stray trailing')))

    def test_SpacedTrailingComments(self):
        self._test(mpl2 + '''
[Strings]
TitleText=Some Title

;Stray trailing comment
;Second stray comment

''', (
            ('Comment', mpl2),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title'),
            ('Comment', 'Second stray comment')))

    def test_TrailingCommentsAndJunk(self):
        self._test(mpl2 + '''
[Strings]
TitleText=Some Title

;Stray trailing comment
Junk
;Second stray comment

''', (
            ('Comment', mpl2),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title'),
            ('Comment', 'Stray trailing'),
            ('Junk', 'Junk'),
            ('Comment', 'Second stray comment')))

    def test_JunkInbetweenEntries(self):
        self._test(mpl2 + '''
[Strings]
TitleText=Some Title

Junk

Good=other string
''', (
            ('Comment', mpl2),
            ('IniSection', 'Strings'),
            ('TitleText', 'Some Title'),
            ('Junk', 'Junk'),
            ('Good', 'other string')))

    def test_empty_file(self):
        self._test('', tuple())
        self._test('\n', (('Whitespace', '\n'),))
        self._test('\n\n', (('Whitespace', '\n\n'),))
        self._test(' \n\n', (('Whitespace', ' \n\n'),))

if __name__ == '__main__':
    unittest.main()
