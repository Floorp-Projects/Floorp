""" Tests for the JS parser. """

import unittest
import jsparagus.lexer
from js_parser.parser import parse_Script, JSParser
from js_parser.lexer import JSLexer


class ESTestCase(unittest.TestCase):
    def parse(self, s):
        if isinstance(s, list):
            f = JSLexer(JSParser())
            for chunk in s:
                f.write(chunk)
            return f.close()
        else:
            return parse_Script(s)

    def assert_parses(self, s):
        self.parse(s)

    def assert_incomplete(self, s):
        """Assert that s fails to parse with UnexpectedEndError.

        (This should be the case if `s` is a prefix of a valid Script.)
        """
        self.assertRaises(jsparagus.lexer.UnexpectedEndError,
                          lambda: parse_Script(s))

    def assert_syntax_error(self, s):
        """Assert that s fails to parse."""
        with self.assertRaises(jsparagus.lexer.SyntaxError):
            parse_Script(s)

    def assert_can_close_after(self, s):
        parser = JSParser()
        lexer = JSLexer(parser)
        if isinstance(s, list):
            for chunk in s:
                lexer.write(chunk)
        else:
            lexer.write(s)
        self.assertTrue(lexer.can_close())

    # === Tests!

    def test_asi_at_end(self):
        self.assert_parses("3 + 4")
        self.assert_syntax_error("3 4")
        self.assert_incomplete("3 +")
        self.assert_incomplete("{")
        self.assert_incomplete("{;")

    def test_asi_at_block_end(self):
        self.assert_parses("{ doCrimes() }")
        self.assert_parses("function f() { ok }")

    def test_asi_after_line_terminator(self):
        self.assert_parses('''\
           switch (value) {
             case 1: break
             case 2: console.log('2');
           }
        ''')
        self.assert_syntax_error(
            "switch (value) { case 1: break case 2: console.log('2'); }")

    def test_asi_after_no_line_terminator_here(self):
        self.assert_parses('''\
           function f() {
             return
               x;
           }
        ''')

    def test_asi_suppressed(self):
        # The specification says ASI does not happen in the production
        # EmptyStatement : `;`.
        self.assert_syntax_error("if (true)")
        self.assert_syntax_error("{ for (;;) }")

        # ASI does not happen in for(;;) loops.
        self.assert_syntax_error("for ( \n ; ) {}")
        self.assert_syntax_error("for ( ; \n ) {}")
        self.assert_syntax_error("for ( \n \n ) {}")
        self.assert_syntax_error("for (var i = 0 \n i < 9;   i++) {}")
        self.assert_syntax_error("for (var i = 0;   i < 9 \n i++) {}")
        self.assert_syntax_error("for (i = 0     \n i < 9;   i++) {}")
        self.assert_syntax_error("for (i = 0;       i < 9 \n i++) {}")
        self.assert_syntax_error("for (let i = 0 \n i < 9;   i++) {}")

        # ASI is suppressed in the production ClassElement[Yield, Await] : `;`
        # to prevent an infinite loop of ASI. lol
        self.assert_syntax_error("class Fail { \n +1; }")

    def test_if_else(self):
        self.assert_parses("if (x) f();")
        self.assert_incomplete("if (x)")
        self.assert_parses("if (x) f(); else g();")
        self.assert_incomplete("if (x) f(); else")
        self.assert_parses("if (x) if (y) g(); else h();")
        self.assert_parses("if (x) if (y) g(); else h(); else j();")

    def test_lexer_decimal(self):
        self.assert_parses("0.")
        self.assert_parses(".5")
        self.assert_syntax_error(".")

    def test_arrow(self):
        self.assert_parses("x => x")
        self.assert_parses("f = x => x;")
        self.assert_parses("(x, y) => [y, x]")
        self.assert_parses("f = (x, y) => {}")
        self.assert_syntax_error("(x, y) => {x: x, y: y}")

    def test_invalid_character(self):
        self.assert_syntax_error("\0")
        self.assert_syntax_error("—x;")
        self.assert_syntax_error("const ONE_THIRD = 1 ÷ 3;")

    def test_regexp(self):
        self.assert_parses(r"/\w/")
        self.assert_parses("/[A-Z]/")
        self.assert_parses("/[//]/")
        self.assert_parses("/a*a/")
        self.assert_parses("/**//x*/")
        self.assert_parses("{} /x/")
        self.assert_parses("of / 2")

    def test_incomplete_comments(self):
        self.assert_syntax_error("/*")
        self.assert_syntax_error("/* hello world")
        self.assert_syntax_error("/* hello world *")
        self.assert_parses(["/* hello\n", " world */"])
        self.assert_parses(["// oawfeoiawj", "ioawefoawjie"])
        self.assert_parses(["// oawfeoiawj", "ioawefoawjie\n ok();"])
        self.assert_parses(["// oawfeoiawj", "ioawefoawjie", "jiowaeawojefiw"])
        self.assert_parses(["// oawfeoiawj", "ioawefoawjie", "jiowaeawojefiw\n ok();"])

    def test_awkward_chunks(self):
        self.assert_parses(["let", "ter.head = 1;"])
        self.assert_parses(["let", " x = 1;"])

        # `list()` here explodes the string into a list of one-character strings.
        self.assert_parses(list("function f() { ok(); }"))

        self.assertEqual(
            self.parse(["/xyzzy/", "g;"]),
            ('script',
             ('script_body',
              ('statement_list_single',
               ('expression_statement',
                ('regexp_literal', '/xyzzy/g'))))))

        self.assertEqual(
            self.parse(['x/', '=2;']),
            ('script',
             ('script_body',
              ('statement_list_single',
               ('expression_statement',
                ('compound_assignment_expr',
                 ('identifier_expr', ('identifier_reference', 'x')),
                 ('box_assign_op', ('div_assign_op', '/=')),
                 ('numeric_literal', '2')))))))

    def test_can_close(self):
        self.assert_can_close_after([])
        self.assert_can_close_after("")
        self.assert_can_close_after("2 + 2;\n")
        self.assert_can_close_after("// seems ok\n")

    def test_can_close_with_asi(self):
        self.assert_can_close_after("2 + 2\n")

    def test_conditional_keywords(self):
        # property names
        self.assert_parses("let obj = {if: 3, function: 4};")
        self.assert_parses("assert(obj.if == 3);")

        # method names
        self.assert_parses("""
            class C {
                if() {}
                function() {}
            }
        """)

        self.assert_parses("var let = [new Date];")    # let as identifier
        self.assert_parses("let v = let;")             # let as keyword, then identifier
        # Next line would fail because the multitoken `let [` lookahead isn't implemented yet.
        # self.assert_parses("let.length;")            # `let .` -> ExpressionStatement
        self.assert_syntax_error("let[0].getYear();")  # `let [` -> LexicalDeclaration

        self.assert_parses("""
            var of = [1, 2, 3];
            for (of of of) console.log(of);  // logs 1, 2, 3
        """)
        self.assert_parses("var of, let, private, target;")
        self.assert_parses("class X { get y() {} }")
        self.assert_parses("async: { break async; }")
        self.assert_parses("var get = { get get() {}, set get(v) {}, set: 3 };")
        self.assert_parses("for (async of => {};;) {}")
        # self.assert_parses("for (async of []) {}")  # would fail


if __name__ == '__main__':
    unittest.main()
