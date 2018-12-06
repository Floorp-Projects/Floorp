# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Tests for the DTD parser.
'''

from __future__ import absolute_import
from __future__ import unicode_literals
import unittest
import re

from compare_locales import parser
from compare_locales.parser import (
    Comment,
    Junk,
    Whitespace,
)
from compare_locales.tests import ParserTestMixin


class TestDTD(ParserTestMixin, unittest.TestCase):
    '''Tests for the DTD Parser.'''
    filename = 'foo.dtd'

    def test_one_entity(self):
        self._test('''<!ENTITY foo.label "stuff">''',
                   (('foo.label', 'stuff'),))
        self.assertListEqual(
            [e.localized for e in self.parser],
            [True]
        )

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
        (Whitespace, '\n'),
        (Junk, '<!ENTITY bad.one "bad " quote">\n'),
        ('good.two', 'two'),
        (Whitespace, '\n'),
        (Junk, '<!ENTITY bad.two "bad "quoted" word">\n'),
        ('good.three', 'three'),
        (Whitespace, '\n'),
        ('good.four', 'good \' quote'),
        (Whitespace, '\n'),
        ('good.five', 'good \'quoted\' word'),
        (Whitespace, '\n'),)

    def test_quotes(self):
        self._test(self.quoteContent, self.quoteRef)

    def test_apos(self):
        qr = re.compile('[\'"]', re.M)

        def quot2apos(s):
            return qr.sub(lambda m: m.group(0) == '"' and "'" or '"', s)

        self._test(quot2apos(self.quoteContent),
                   ((ref[0], quot2apos(ref[1])) for ref in self.quoteRef))

    def test_parsed_ref(self):
        self._test('''<!ENTITY % fooDTD SYSTEM "chrome://brand.dtd">
  %fooDTD;
''',
                   (('fooDTD', '"chrome://brand.dtd"'),))
        self._test('''<!ENTITY  %  fooDTD  SYSTEM  "chrome://brand.dtd">
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
                   (
                       ('first', 'string'),
                       (Whitespace, '\n'),
                       ('second', 'string'),
                       (Whitespace, '\n'),
                       (Comment, 'out'),
                       (Whitespace, '\n')))

    def test_license_header(self):
        p = parser.getParser('foo.dtd')
        p.readContents(self.resource('triple-license.dtd'))
        entities = list(p.walk())
        self.assertIsInstance(entities[0], parser.Comment)
        self.assertIn('MPL', entities[0].all)
        e = entities[2]
        self.assertIsInstance(e, parser.Entity)
        self.assertEqual(e.key, 'foo')
        self.assertEqual(e.val, 'value')
        self.assertEqual(len(entities), 4)
        p.readContents(b'''\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/.  -->

<!ENTITY foo "value">
''')
        entities = list(p.walk())
        self.assertIsInstance(entities[0], parser.Comment)
        self.assertIn('MPL', entities[0].all)
        e = entities[2]
        self.assertIsInstance(e, parser.Entity)
        self.assertEqual(e.key, 'foo')
        self.assertEqual(e.val, 'value')
        self.assertEqual(len(entities), 4)
        # Test again without empty line after licence header, and with BOM.
        p.readContents(b'''\xEF\xBB\xBF\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/.  -->
<!ENTITY foo "value">
''')
        entities = list(p.walk())
        self.assertIsInstance(entities[0], parser.Comment)
        self.assertIn('MPL', entities[0].all)
        e = entities[2]
        self.assertIsInstance(e, parser.Entity)
        self.assertEqual(e.key, 'foo')
        self.assertEqual(e.val, 'value')
        self.assertEqual(len(entities), 4)

    def testBOM(self):
        self._test(u'\ufeff<!ENTITY foo.label "stuff">',
                   (('foo.label', 'stuff'),))

    def test_trailing_whitespace(self):
        self._test('<!ENTITY foo.label "stuff">\n  \n',
                   (('foo.label', 'stuff'), (Whitespace, '\n  \n')))

    def test_unicode_comment(self):
        self._test(b'<!-- \xe5\x8f\x96 -->'.decode('utf-8'),
                   ((Comment, u'\u53d6'),))

    def test_empty_file(self):
        self._test('', tuple())
        self._test('\n', ((Whitespace, '\n'),))
        self._test('\n\n', ((Whitespace, '\n\n'),))
        self._test(' \n\n', ((Whitespace, ' \n\n'),))

    def test_positions(self):
        self.parser.readContents(b'''\
<!ENTITY one  "value">
<!ENTITY  two "other
escaped value">
''')
        one, two = list(self.parser)
        self.assertEqual(one.position(), (1, 1))
        self.assertEqual(one.value_position(), (1, 16))
        self.assertEqual(one.position(-1), (1, 23))
        self.assertEqual(two.position(), (2, 1))
        self.assertEqual(two.value_position(), (2, 16))
        self.assertEqual(two.value_position(-1), (3, 14))
        self.assertEqual(two.value_position(10), (3, 5))

    def test_word_count(self):
        self.parser.readContents(b'''\
<!ENTITY a "one">
<!ENTITY b "one<br>two">
<!ENTITY c "one<span>word</span>">
<!ENTITY d "one <a href='foo'>two</a> three">
''')
        a, b, c, d = list(self.parser)
        self.assertEqual(a.count_words(), 1)
        self.assertEqual(b.count_words(), 2)
        self.assertEqual(c.count_words(), 1)
        self.assertEqual(d.count_words(), 3)

    def test_html_entities(self):
        self.parser.readContents(b'''\
<!ENTITY named "&amp;">
<!ENTITY numcode "&#38;">
<!ENTITY shorthexcode "&#x26;">
<!ENTITY longhexcode "&#x0026;">
<!ENTITY unknown "&unknownEntity;">
''')
        entities = iter(self.parser)

        entity = next(entities)
        self.assertEqual(entity.raw_val, '&amp;')
        self.assertEqual(entity.val, '&')

        entity = next(entities)
        self.assertEqual(entity.raw_val, '&#38;')
        self.assertEqual(entity.val, '&')

        entity = next(entities)
        self.assertEqual(entity.raw_val, '&#x26;')
        self.assertEqual(entity.val, '&')

        entity = next(entities)
        self.assertEqual(entity.raw_val, '&#x0026;')
        self.assertEqual(entity.val, '&')

        entity = next(entities)
        self.assertEqual(entity.raw_val, '&unknownEntity;')
        self.assertEqual(entity.val, '&unknownEntity;')

    def test_comment_val(self):
        self.parser.readContents(b'''\
<!-- comment
spanning lines -->  <!--
-->
<!-- last line -->
''')
        entities = self.parser.walk()

        entity = next(entities)
        self.assertIsInstance(entity, parser.Comment)
        self.assertEqual(entity.val, ' comment\nspanning lines ')
        entity = next(entities)
        self.assertIsInstance(entity, parser.Whitespace)

        entity = next(entities)
        self.assertIsInstance(entity, parser.Comment)
        self.assertEqual(entity.val, '\n')
        entity = next(entities)
        self.assertIsInstance(entity, parser.Whitespace)

        entity = next(entities)
        self.assertIsInstance(entity, parser.Comment)
        self.assertEqual(entity.val, ' last line ')
        entity = next(entities)
        self.assertIsInstance(entity, parser.Whitespace)

    def test_pre_comment(self):
        self.parser.readContents(b'''\
<!-- comment -->
<!ENTITY one "string">

<!-- standalone -->

<!-- glued --><!ENTITY second "string">
''')
        entities = self.parser.walk()

        entity = next(entities)
        self.assertIsInstance(entity.pre_comment, parser.Comment)
        self.assertEqual(entity.pre_comment.val, ' comment ')
        entity = next(entities)
        self.assertIsInstance(entity, parser.Whitespace)

        entity = next(entities)
        self.assertIsInstance(entity, parser.Comment)
        self.assertEqual(entity.val, ' standalone ')
        entity = next(entities)
        self.assertIsInstance(entity, parser.Whitespace)

        entity = next(entities)
        self.assertIsInstance(entity.pre_comment, parser.Comment)
        self.assertEqual(entity.pre_comment.val, ' glued ')
        entity = next(entities)
        self.assertIsInstance(entity, parser.Whitespace)
        with self.assertRaises(StopIteration):
            next(entities)


if __name__ == '__main__':
    unittest.main()
