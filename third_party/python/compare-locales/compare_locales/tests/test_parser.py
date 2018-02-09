# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import textwrap
import unittest

from compare_locales import parser


class TestParserContext(unittest.TestCase):
    def test_linecol(self):
        "Should return 1-based line and column numbers."
        ctx = parser.Parser.Context('''first line
second line
third line
''')
        self.assertEqual(
            ctx.linecol(0),
            (1, 1)
        )
        self.assertEqual(
            ctx.linecol(1),
            (1, 2)
        )
        self.assertEqual(
            ctx.linecol(len('first line')),
            (1, len('first line') + 1)
        )
        self.assertEqual(
            ctx.linecol(len('first line') + 1),
            (2, 1)
        )
        self.assertEqual(
            ctx.linecol(len(ctx.contents)),
            (4, 1)
        )

    def test_empty_parser(self):
        p = parser.Parser()
        entities, _map = p.parse()
        self.assertListEqual(
            entities,
            []
        )
        self.assertDictEqual(
            _map,
            {}
        )


class TestOffsetComment(unittest.TestCase):
    def test_offset(self):
        ctx = parser.Parser.Context(textwrap.dedent('''\
            #foo
            #bar
            # baz
            '''
        ))  # noqa
        offset_comment = parser.OffsetComment(ctx, (0, len(ctx.contents)))
        self.assertEqual(
            offset_comment.val,
            textwrap.dedent('''\
                foo
                bar
                 baz
            ''')
        )
