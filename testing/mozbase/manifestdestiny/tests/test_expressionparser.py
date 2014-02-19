#!/usr/bin/env python

import unittest
from manifestparser import parse

class ExpressionParserTest(unittest.TestCase):
    """Test the conditional expression parser."""

    def test_basic(self):

        self.assertEqual(parse("1"), 1)
        self.assertEqual(parse("100"), 100)
        self.assertEqual(parse("true"), True)
        self.assertEqual(parse("false"), False)
        self.assertEqual('', parse('""'))
        self.assertEqual(parse('"foo bar"'), 'foo bar')
        self.assertEqual(parse("'foo bar'"), 'foo bar')
        self.assertEqual(parse("foo", foo=1), 1)
        self.assertEqual(parse("bar", bar=True), True)
        self.assertEqual(parse("abc123", abc123="xyz"), 'xyz')

    def test_equality(self):

        self.assertTrue(parse("true == true"))
        self.assertTrue(parse("false == false"))
        self.assertTrue(parse("1 == 1"))
        self.assertTrue(parse("100 == 100"))
        self.assertTrue(parse('"some text" == "some text"'))
        self.assertTrue(parse("true != false"))
        self.assertTrue(parse("1 != 2"))
        self.assertTrue(parse('"text" != "other text"'))
        self.assertTrue(parse("foo == true", foo=True))
        self.assertTrue(parse("foo == 1", foo=1))
        self.assertTrue(parse('foo == "bar"', foo='bar'))
        self.assertTrue(parse("foo == bar", foo=True, bar=True))
        self.assertTrue(parse("true == foo", foo=True))
        self.assertTrue(parse("foo != true", foo=False))
        self.assertTrue(parse("foo != 2", foo=1))
        self.assertTrue(parse('foo != "bar"', foo='abc'))
        self.assertTrue(parse("foo != bar", foo=True, bar=False))
        self.assertTrue(parse("true != foo", foo=False))
        self.assertTrue(parse("!false"))

    def test_conjunctures(self):
        self.assertTrue(parse("true && true"))
        self.assertTrue(parse("true || false"))
        self.assertFalse(parse("false || false"))
        self.assertFalse(parse("true && false"))
        self.assertTrue(parse("true || false && false"))

    def test_parentheses(self):
        self.assertTrue(parse("(true)"))
        self.assertEqual(parse("(10)"), 10)
        self.assertEqual(parse('("foo")'), 'foo')
        self.assertEqual(parse("(foo)", foo=1), 1)
        self.assertTrue(parse("(true == true)"), True)
        self.assertTrue(parse("(true != false)"))
        self.assertTrue(parse("(true && true)"))
        self.assertTrue(parse("(true || false)"))
        self.assertTrue(parse("(true && true || false)"))
        self.assertFalse(parse("(true || false) && false"))
        self.assertTrue(parse("(true || false) && true"))
        self.assertTrue(parse("true && (true || false)"))
        self.assertTrue(parse("true && (true || false)"))
        self.assertTrue(parse("(true && false) || (true && (true || false))"))

    def test_comments(self):
        # comments in expressions work accidentally, via an implementation
        # detail - the '#' character doesn't match any of the regular
        # expressions we specify as tokens, and thus are ignored.
        # However, having explicit tests for them means that should the
        # implementation ever change, comments continue to work, even if that
        # means a new implementation must handle them explicitly.
        self.assertTrue(parse("true == true # it does!"))
        self.assertTrue(parse("false == false # it does"))
        self.assertTrue(parse("false != true # it doesnt"))
        self.assertTrue(parse('"string with #" == "string with #" # really, it does'))
        self.assertTrue(parse('"string with #" != "string with # but not the same" # no match!'))

if __name__ == '__main__':
    unittest.main()
