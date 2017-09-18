# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales.tests import ParserTestMixin


mpl2 = '''\
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
'''


class TestDefinesParser(ParserTestMixin, unittest.TestCase):

    filename = 'defines.inc'

    def testBrowser(self):
        self._test(mpl2 + '''#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org

# If non-English locales wish to credit multiple contributors, uncomment this
# variable definition and use the format specified.
# #define MOZ_LANGPACK_CONTRIBUTORS <em:contributor>Joe Solon</em:contributor>

#unfilter emptyLines

''', (
            ('Comment', mpl2),
            ('DefinesInstruction', 'filter emptyLines'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            ('Comment', '#define'),
            ('DefinesInstruction', 'unfilter emptyLines')))

    def testBrowserWithContributors(self):
        self._test(mpl2 + '''#filter emptyLines

#define MOZ_LANGPACK_CREATOR mozilla.org

# If non-English locales wish to credit multiple contributors, uncomment this
# variable definition and use the format specified.
#define MOZ_LANGPACK_CONTRIBUTORS <em:contributor>Joe Solon</em:contributor>

#unfilter emptyLines

''', (
            ('Comment', mpl2),
            ('DefinesInstruction', 'filter emptyLines'),
            ('MOZ_LANGPACK_CREATOR', 'mozilla.org'),
            ('Comment', 'non-English'),
            ('MOZ_LANGPACK_CONTRIBUTORS',
             '<em:contributor>Joe Solon</em:contributor>'),
            ('DefinesInstruction', 'unfilter emptyLines')))

    def testCommentWithNonAsciiCharacters(self):
        self._test(mpl2 + '''#filter emptyLines

# e.g. #define seamonkey_l10n <DT><A HREF="urn:foo">SeaMonkey v češtině</a>
#define seamonkey_l10n_long

#unfilter emptyLines

''', (
            ('Comment', mpl2),
            ('DefinesInstruction', 'filter emptyLines'),
            ('Comment', u'češtině'),
            ('seamonkey_l10n_long', ''),
            ('DefinesInstruction', 'unfilter emptyLines')))

    def testToolkit(self):
        self._test('''#define MOZ_LANG_TITLE English (US)
''', (
            ('MOZ_LANG_TITLE', 'English (US)'),))

    def testToolkitEmpty(self):
        self._test('', tuple())

    def test_empty_file(self):
        '''Test that empty files generate errors

        defines.inc are interesting that way, as their
        content is added to the generated file.
        '''
        self._test('\n', (('Junk', '\n'),))
        self._test('\n\n', (('Junk', '\n\n'),))
        self._test(' \n\n', (('Junk', ' \n\n'),))


if __name__ == '__main__':
    unittest.main()
