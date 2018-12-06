# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import unittest

from compare_locales.tests import ParserTestMixin
from compare_locales.parser import (
    BadEntity,
    Whitespace,
)


class TestPoParser(ParserTestMixin, unittest.TestCase):
    maxDiff = None
    filename = 'strings.po'

    def test_parse_string_list(self):
        self.parser.readUnicode('  ')
        ctx = self.parser.ctx
        with self.assertRaises(BadEntity):
            self.parser._parse_string_list(ctx, 0, 'msgctxt')
        self.parser.readUnicode('msgctxt   ')
        ctx = self.parser.ctx
        with self.assertRaises(BadEntity):
            self.parser._parse_string_list(ctx, 0, 'msgctxt')
        self.parser.readUnicode('msgctxt " "')
        ctx = self.parser.ctx
        self.assertTupleEqual(
            self.parser._parse_string_list(ctx, 0, 'msgctxt'),
            (" ", len(ctx.contents))
        )
        self.parser.readUnicode('msgctxt " " \t "A"\r "B"asdf')
        ctx = self.parser.ctx
        self.assertTupleEqual(
            self.parser._parse_string_list(ctx, 0, 'msgctxt'),
            (" AB", len(ctx.contents)-4)
        )
        self.parser.readUnicode('msgctxt "\\\\ " "A" "B"asdf"fo"')
        ctx = self.parser.ctx
        self.assertTupleEqual(
            self.parser._parse_string_list(ctx, 0, 'msgctxt'),
            ("\\ AB", len(ctx.contents)-8)
        )

    def test_simple_string(self):
        source = '''
msgid "untranslated string"
msgstr "translated string"
'''
        self._test(
            source,
            (
                (Whitespace, '\n'),
                (('untranslated string', None), 'translated string'),
                (Whitespace, '\n'),
            )
        )

    def test_escapes(self):
        source = r'''
msgid "untranslated string"
msgstr "\\\t\r\n\""
'''
        self._test(
            source,
            (
                (Whitespace, '\n'),
                (('untranslated string', None), '\\\t\r\n"'),
                (Whitespace, '\n'),
            )
        )

    def test_comments(self):
        source = '''
#  translator-comments
#. extracted-comments
#: reference...
#, flag...
#| msgctxt previous-context
#| msgid previous-untranslated-string
msgid "untranslated string"
msgstr "translated string"
'''
        self._test(
            source,
            (
                (Whitespace, '\n'),
                (
                    ('untranslated string', None),
                    'translated string',
                    'extracted-comments',
                ),
                (Whitespace, '\n'),
            )
        )

    def test_simple_context(self):
        source = '''
msgctxt "context to use"
msgid "untranslated string"
msgstr "translated string"
'''
        self._test(
            source,
            (
                (Whitespace, '\n'),
                (
                    ('untranslated string', 'context to use'),
                    'translated string'
                ),
                (Whitespace, '\n'),
            )
        )

    def test_translated(self):
        source = '''
msgid "reference 1"
msgstr "translated string"

msgid "reference 2"
msgstr ""
'''
        self._test(
            source,
            (
                (Whitespace, '\n'),
                (('reference 1', None), 'translated string'),
                (Whitespace, '\n'),
                (('reference 2', None), ''),
                (Whitespace, '\n'),
            )
        )
        entities = self.parser.parse()
        self.assertListEqual(
            [e.localized for e in entities],
            [True, False]
        )
