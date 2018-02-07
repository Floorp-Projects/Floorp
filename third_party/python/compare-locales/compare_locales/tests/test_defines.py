# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales.tests import ParserTestMixin
from compare_locales.parser import (
    Comment,
    DefinesInstruction,
    Junk,
    Whitespace,
)


mpl2 = '''\
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.'''


class TestDefinesParser(ParserTestMixin, unittest.TestCase):

    filename = 'defines.inc'

    def testBrowser(self):
        self._test(mpl2 + '''
#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org

# If non-English locales wish to credit multiple contributors, uncomment this
# variable definition and use the format specified.
# #define MOZ_LANGPACK_CONTRIBUTORS <em:contributor>Joe Solon</em:contributor>

#unfilter emptyLines

''', (
            (Comment, mpl2),
            (Whitespace, '\n'),
            (DefinesInstruction, 'filter emptyLines'),
            (Whitespace, '\n\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n\n'),
            (Comment, '#define'),
            (Whitespace, '\n\n'),
            (DefinesInstruction, 'unfilter emptyLines'),
            (Junk, '\n\n')))

    def testBrowserWithContributors(self):
        self._test(mpl2 + '''
#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org

# If non-English locales wish to credit multiple contributors, uncomment this
# variable definition and use the format specified.
#define MOZ_LANGPACK_CONTRIBUTORS <em:contributor>Joe Solon</em:contributor>

#unfilter emptyLines

''', (
            (Comment, mpl2),
            (Whitespace, '\n'),
            (DefinesInstruction, 'filter emptyLines'),
            (Whitespace, '\n\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n\n'),
            (Comment, 'non-English'),
            (Whitespace, '\n'),
            ('MOZ_LANGPACK_CONTRIBUTORS',
             '<em:contributor>Joe Solon</em:contributor>'),
            (Whitespace, '\n\n'),
            (DefinesInstruction, 'unfilter emptyLines'),
            (Junk, '\n\n')))

    def testCommentWithNonAsciiCharacters(self):
        self._test(mpl2 + '''
#filter emptyLines

# e.g. #define seamonkey_l10n <DT><A HREF="urn:foo">SeaMonkey v češtině</a>
#define seamonkey_l10n_long

#unfilter emptyLines

''', (
            (Comment, mpl2),
            (Whitespace, '\n'),
            (DefinesInstruction, 'filter emptyLines'),
            (Whitespace, '\n\n'),
            (Comment, u'češtině'),
            (Whitespace, '\n'),
            ('seamonkey_l10n_long', ''),
            (Whitespace, '\n\n'),
            (DefinesInstruction, 'unfilter emptyLines'),
            (Junk, '\n\n')))

    def test_no_empty_lines(self):
        self._test('''#define MOZ_LANGPACK_CREATOR mozilla.org
#define MOZ_LANGPACK_CREATOR mozilla.org
''', (
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n')))

    def test_empty_line_between(self):
        self._test('''#define MOZ_LANGPACK_CREATOR mozilla.org

#define MOZ_LANGPACK_CREATOR mozilla.org
''', (
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Junk, '\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n')))

    def test_empty_line_at_the_beginning(self):
        self._test('''
#define MOZ_LANGPACK_CREATOR mozilla.org
#define MOZ_LANGPACK_CREATOR mozilla.org
''', (
            (Junk, '\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n')))

    def test_filter_empty_lines(self):
        self._test('''#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org
#define MOZ_LANGPACK_CREATOR mozilla.org
#unfilter emptyLines''', (
            (DefinesInstruction, 'filter emptyLines'),
            (Whitespace, '\n\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n'),
            (DefinesInstruction, 'unfilter emptyLines')))

    def test_unfilter_empty_lines_with_trailing(self):
        self._test('''#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org
#define MOZ_LANGPACK_CREATOR mozilla.org
#unfilter emptyLines
''', (
            (DefinesInstruction, 'filter emptyLines'),
            (Whitespace, '\n\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            (Whitespace, '\n'),
            (DefinesInstruction, 'unfilter emptyLines'),
            (Whitespace, '\n')))

    def testToolkit(self):
        self._test('''#define MOZ_LANG_TITLE English (US)
''', (
            ('MOZ_LANG_TITLE', 'English (US)'),
            (Whitespace, '\n')))

    def testToolkitEmpty(self):
        self._test('', tuple())

    def test_empty_file(self):
        '''Test that empty files generate errors

        defines.inc are interesting that way, as their
        content is added to the generated file.
        '''
        self._test('\n', ((Junk, '\n'),))
        self._test('\n\n', ((Junk, '\n\n'),))
        self._test(' \n\n', ((Junk, ' \n\n'),))

    def test_whitespace_value(self):
        '''Test that there's only one whitespace between key and value
        '''
        # funny formatting of trailing whitespace to make it explicit
        # and flake-8 happy
        self._test('''\
#define one \n\
#define two  \n\
#define tre   \n\
''', (
            ('one', ''),
            (Whitespace, '\n'),
            ('two', ' '),
            (Whitespace, '\n'),
            ('tre', '  '),
            (Whitespace, '\n'),))


if __name__ == '__main__':
    unittest.main()
