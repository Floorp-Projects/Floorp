# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Tests for the DTD parser.
'''

import unittest
import re

from compare_locales import parser
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
        ('Junk', '<!ENTITY bad.one "bad " quote">'),
        ('good.two', 'two'),
        ('Junk', '<!ENTITY bad.two "bad "quoted" word">'),
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
                   (('first', 'string'), ('second', 'string'),
                    ('Comment', 'out')))

    def test_license_header(self):
        p = parser.getParser('foo.dtd')
        p.readContents(self.resource('triple-license.dtd'))
        entities = list(p.walk())
        self.assert_(isinstance(entities[0], parser.Comment))
        self.assertIn('MPL', entities[0].all)
        e = entities[1]
        self.assert_(isinstance(e, parser.Entity))
        self.assertEqual(e.key, 'foo')
        self.assertEqual(e.val, 'value')
        self.assertEqual(len(entities), 2)
        p.readContents('''\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/.  -->
<!ENTITY foo "value">
''')
        entities = list(p.walk())
        self.assert_(isinstance(entities[0], parser.Comment))
        self.assertIn('MPL', entities[0].all)
        e = entities[1]
        self.assert_(isinstance(e, parser.Entity))
        self.assertEqual(e.key, 'foo')
        self.assertEqual(e.val, 'value')
        self.assertEqual(len(entities), 2)

    def testBOM(self):
        self._test(u'\ufeff<!ENTITY foo.label "stuff">'.encode('utf-8'),
                   (('foo.label', 'stuff'),))

    def test_trailing_whitespace(self):
        self._test('<!ENTITY foo.label "stuff">\n  \n',
                   (('foo.label', 'stuff'),))

    def test_unicode_comment(self):
        self._test('<!-- \xe5\x8f\x96 -->',
                   (('Comment', u'\u53d6'),))

    def test_empty_file(self):
        self._test('', tuple())
        self._test('\n', (('Whitespace', '\n'),))
        self._test('\n\n', (('Whitespace', '\n\n'),))
        self._test(' \n\n', (('Whitespace', ' \n\n'),))

    def test_positions(self):
        self.parser.readContents('''\
<!ENTITY one  "value">
<!ENTITY  two "other
escaped value">
''')
        one, two = list(self.parser)
        self.assertEqual(one.position(), (1, 1))
        self.assertEqual(one.value_position(), (1, 16))
        self.assertEqual(one.position(-1), (2, 1))
        self.assertEqual(two.position(), (2, 1))
        self.assertEqual(two.value_position(), (2, 16))
        self.assertEqual(two.value_position(-1), (3, 14))
        self.assertEqual(two.value_position(10), (3, 5))

    def test_post(self):
        self.parser.readContents('<!ENTITY a "a"><!ENTITY b "b">')
        a, b = list(self.parser)
        self.assertEqual(a.post, '')
        self.parser.readContents('<!ENTITY a "a"> <!ENTITY b "b">')
        a, b = list(self.parser)
        self.assertEqual(a.post, ' ')


if __name__ == '__main__':
    unittest.main()
