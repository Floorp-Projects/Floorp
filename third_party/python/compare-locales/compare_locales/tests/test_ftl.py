# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest

from compare_locales import parser
from compare_locales.tests import ParserTestMixin


class TestFluentParser(ParserTestMixin, unittest.TestCase):
    maxDiff = None
    filename = 'foo.ftl'

    def test_equality_same(self):
        source = 'progress = Progress: { NUMBER($num, style: "percent") }.'

        self.parser.readContents(source)
        [ent1] = list(self.parser)

        self.parser.readContents(source)
        [ent2] = list(self.parser)

        self.assertTrue(ent1.equals(ent2))

    def test_equality_different_whitespace(self):
        source1 = 'foo = { $arg }'
        source2 = 'foo = {    $arg    }'

        self.parser.readContents(source1)
        [ent1] = list(self.parser)

        self.parser.readContents(source2)
        [ent2] = list(self.parser)

        self.assertTrue(ent1.equals(ent2))

    def test_word_count(self):
        self.parser.readContents('''\
a = One
b = One two three
c = One { $arg } two
d =
    One { $arg ->
       *[x] Two three
        [y] Four
    } five.
e =
    .attr = One
f =
    .attr1 = One
    .attr2 = Two
g = One two
    .attr = Three
h =
    One { $arg ->
       *[x] Two three
        [y] Four
    } five.
    .attr1 =
        Six { $arg ->
           *[x] Seven eight
            [y] Nine
        } ten.
''')

        a, b, c, d, e, f, g, h = list(self.parser)
        self.assertEqual(a.count_words(), 1)
        self.assertEqual(b.count_words(), 3)
        self.assertEqual(c.count_words(), 2)
        self.assertEqual(d.count_words(), 5)
        self.assertEqual(e.count_words(), 1)
        self.assertEqual(f.count_words(), 2)
        self.assertEqual(g.count_words(), 3)
        self.assertEqual(h.count_words(), 10)

    def test_simple_message(self):
        self.parser.readContents('a = A')

        [a] = list(self.parser)
        self.assertEqual(a.key, 'a')
        self.assertEqual(a.val, 'A')
        self.assertEqual(a.all, 'a = A')
        attributes = list(a.attributes)
        self.assertEqual(len(attributes), 0)

    def test_complex_message(self):
        self.parser.readContents('abc = A { $arg } B { msg } C')

        [abc] = list(self.parser)
        self.assertEqual(abc.key, 'abc')
        self.assertEqual(abc.val, 'A { $arg } B { msg } C')
        self.assertEqual(abc.all, 'abc = A { $arg } B { msg } C')

    def test_multiline_message(self):
        self.parser.readContents('''\
abc =
    A
    B
    C
''')

        [abc] = list(self.parser)
        self.assertEqual(abc.key, 'abc')
        self.assertEqual(abc.val, 'A\n    B\n    C')
        self.assertEqual(abc.all, 'abc =\n    A\n    B\n    C')

    def test_message_with_attribute(self):
        self.parser.readContents('''\
abc = ABC
    .attr = Attr
''')

        [abc] = list(self.parser)
        self.assertEqual(abc.key, 'abc')
        self.assertEqual(abc.val, 'ABC')
        self.assertEqual(abc.all, 'abc = ABC\n    .attr = Attr')

    def test_message_with_attribute_and_no_value(self):
        self.parser.readContents('''\
abc
    .attr = Attr
''')

        [abc] = list(self.parser)
        self.assertEqual(abc.key, 'abc')
        self.assertEqual(abc.val, None)
        self.assertEqual(abc.all, 'abc\n    .attr = Attr')
        attributes = list(abc.attributes)
        self.assertEqual(len(attributes), 1)
        attr = attributes[0]
        self.assertEqual(attr.key, 'attr')
        self.assertEqual(attr.val, 'Attr')

    def test_non_localizable(self):
        self.parser.readContents('''\
### Resource Comment

foo = Foo

## Group Comment

-bar = Bar

##

# Standalone Comment

# Baz Comment
baz = Baz
''')
        entities = self.parser.walk()

        entity = next(entities)
        self.assertTrue(isinstance(entity,  parser.FluentComment))
        self.assertEqual(entity.all, '### Resource Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.FluentMessage))
        self.assertEqual(entity.val, 'Foo')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity,  parser.FluentComment))
        self.assertEqual(entity.all, '## Group Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.FluentTerm))
        self.assertEqual(entity.val, 'Bar')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity,  parser.FluentComment))
        self.assertEqual(entity.all, '##')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity,  parser.FluentComment))
        self.assertEqual(entity.all, '# Standalone Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.FluentMessage))
        self.assertEqual(entity.val, 'Baz')
        self.assertEqual(entity.entry.comment.content, 'Baz Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n')

    def test_non_localizable_syntax_zero_four(self):
        self.parser.readContents('''\
// Resource Comment

foo = Foo

// Section Comment
[[ Section Header ]]

bar = Bar

[[ Another Section ]]

// Standalone Comment

// Baz Comment
baz = Baz
''')
        entities = self.parser.walk()

        entity = next(entities)
        self.assertTrue(isinstance(entity,  parser.FluentComment))
        self.assertEqual(entity.all, '// Resource Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.FluentEntity))
        self.assertEqual(entity.val, 'Foo')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity,  parser.FluentComment))
        self.assertEqual(
            entity.all,
            '// Section Comment\n[[ Section Header ]]'
        )

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.FluentEntity))
        self.assertEqual(entity.val, 'Bar')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity,  parser.FluentComment))
        self.assertEqual(entity.all, '[[ Another Section ]]')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity,  parser.FluentComment))
        self.assertEqual(entity.all, '// Standalone Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n\n')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.FluentEntity))
        self.assertEqual(entity.val, 'Baz')
        self.assertEqual(entity.entry.comment.content, 'Baz Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
        self.assertEqual(entity.all, '\n')

    def test_comments_val(self):
        self.parser.readContents('''\
// Legacy Comment

### Resource Comment

## Section Comment

# Standalone Comment
''')
        entities = self.parser.walk()

        entity = next(entities)
        # ensure that fluent comments are FluentComments and Comments
        self.assertTrue(isinstance(entity,  parser.FluentComment))

        # now test the actual .val values
        self.assertTrue(isinstance(entity,   parser.Comment))
        self.assertEqual(entity.val, 'Legacy Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))

        entity = next(entities)
        self.assertTrue(isinstance(entity,   parser.Comment))
        self.assertEqual(entity.val, 'Resource Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))

        entity = next(entities)
        self.assertTrue(isinstance(entity,   parser.Comment))
        self.assertEqual(entity.val, 'Section Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))

        entity = next(entities)
        self.assertTrue(isinstance(entity,   parser.Comment))
        self.assertEqual(entity.val, 'Standalone Comment')

        entity = next(entities)
        self.assertTrue(isinstance(entity, parser.Whitespace))
