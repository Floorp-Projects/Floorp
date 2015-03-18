# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Tests for the DTD parser.
'''

import unittest
import re

from compare_locales.parser import getParser
from compare_locales.tests import ParserTestMixin


class TestDTD(ParserTestMixin, unittest.TestCase):
    '''Tests for the DTD Parser.'''
    filename = 'foo.dtd'

    def test_one_entity(self):
        self._test('''<!ENTITY foo.label "stuff">''',
                   (('foo.label', 'stuff'),))

    quoteContent = '''<!ENTITY good.one "one">
<!ENTITY bad.one "bad " quote">
<!ENTITY good.two "two">
<!ENTITY bad.two "bad "quoted" word">
<!ENTITY good.three "three">
<!ENTITY good.four "good ' quote">
<!ENTITY good.five "good 'quoted' word">
'''
    quoteRef = (
        ('good.one', 'one'),
        ('_junk_\\d_25-56$', '<!ENTITY bad.one "bad " quote">'),
        ('good.two', 'two'),
        ('_junk_\\d_82-119$', '<!ENTITY bad.two "bad "quoted" word">'),
        ('good.three', 'three'),
        ('good.four', 'good \' quote'),
        ('good.five', 'good \'quoted\' word'),)

    def test_quotes(self):
        self._test(self.quoteContent, self.quoteRef)

    def test_apos(self):
        qr = re.compile('[\'"]', re.M)

        def quot2apos(s):
            return qr.sub(lambda m: m.group(0) == '"' and "'" or '"', s)

        self._test(quot2apos(self.quoteContent),
                   map(lambda t: (t[0], quot2apos(t[1])), self.quoteRef))

    def test_parsed_ref(self):
        self._test('''<!ENTITY % fooDTD SYSTEM "chrome://brand.dtd">
  %fooDTD;
''',
                   (('fooDTD', '"chrome://brand.dtd"'),))

    def test_trailing_comment(self):
        self._test('''<!ENTITY first "string">
<!ENTITY second "string">
<!--
<!ENTITY commented "out">
-->
''',
                   (('first', 'string'), ('second', 'string')))

    def test_license_header(self):
        p = getParser('foo.dtd')
        p.readContents(self.resource('triple-license.dtd'))
        for e in p:
            self.assertEqual(e.key, 'foo')
            self.assertEqual(e.val, 'value')
        self.assert_('MPL' in p.header)
        p.readContents('''\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/.  -->
<!ENTITY foo "value">
''')
        for e in p:
            self.assertEqual(e.key, 'foo')
            self.assertEqual(e.val, 'value')
        self.assert_('MPL' in p.header)

if __name__ == '__main__':
    unittest.main()
