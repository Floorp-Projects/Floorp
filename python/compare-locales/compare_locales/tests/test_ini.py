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
''', (('TitleText', 'Some Title'),))
        self.assert_('UTF-8' in self.parser.header)

    def testMPL2_Space_UTF(self):
        self._test(mpl2 + '''
; This file is in the UTF-8 encoding
[Strings]
TitleText=Some Title
''', (('TitleText', 'Some Title'),))
        self.assert_('MPL' in self.parser.header)

    def testMPL2_Space(self):
        self._test(mpl2 + '''
[Strings]
TitleText=Some Title
''', (('TitleText', 'Some Title'),))
        self.assert_('MPL' in self.parser.header)

    def testMPL2_MultiSpace(self):
        self._test(mpl2 + '''\

; more comments

[Strings]
TitleText=Some Title
''', (('TitleText', 'Some Title'),))
        self.assert_('MPL' in self.parser.header)

    def testMPL2_Junk(self):
        self._test(mpl2 + '''\
Junk
[Strings]
TitleText=Some Title
''', (('_junk_\\d+_0-213$', mpl2 + '''\
Junk
[Strings]'''), ('TitleText', 'Some Title')))
        self.assert_('MPL' not in self.parser.header)

if __name__ == '__main__':
    unittest.main()
