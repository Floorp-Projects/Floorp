/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.lexer

import mozilla.components.lib.jexl.grammar.Grammar
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class LexerTest {
    private lateinit var lexer: Lexer

    @Before
    fun setUp() {
        lexer = Lexer(Grammar())
    }

    @Test
    fun `should count a string as one element`() {
        val expression = "\"foo\""

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.LITERAL,
                    "\"foo\"",
                    "foo",
                ),
            ),
        )
    }

    @Test
    fun `should support single-quote strings`() {
        val expression = "'foo'"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "'foo'", "foo"),
            ),
        )
    }

    @Test
    fun `should find multiple strings`() {
        val expression = "\"foo\" 'bar' \"baz\""

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.LITERAL,
                    "\"foo\"",
                    "foo",
                ),
                Token(
                    Token.Type.LITERAL,
                    "'bar'",
                    "bar",
                ),
                Token(
                    Token.Type.LITERAL,
                    "\"baz\"",
                    "baz",
                ),
            ),
        )
    }

    @Test
    fun `should support escaping double-quotes`() {
        val expression = "\"f\\\"oo\""

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.LITERAL,
                    "\"f\\\"oo\"",
                    "f\"oo",
                ),
            ),
        )
    }

    @Test
    fun `should support escaping single-quotes`() {
        val expression = "'f\\'oo'"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.LITERAL,
                    "'f\\'oo'",
                    "f'oo",
                ),
            ),
        )
    }

    @Test
    fun `should count an identifier as one element`() {
        val expression = "alpha12345"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.IDENTIFIER,
                    "alpha12345",
                    "alpha12345",
                ),
            ),
        )
    }

    @Test
    fun `should support boolean true`() {
        val expression = "true"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "true", true),
            ),
        )
    }

    @Test
    fun `should support boolean false`() {
        val expression = "false"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "false", false),
            ),
        )
    }

    @Test
    fun `should support comments`() {
        val expression = "# This is a comment"

        assertExpressionYieldsTokens(expression, emptyList())
    }

    @Test
    fun `should support comments after expressions`() {
        val expression = "true false #This is a comment"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "true", true),
                Token(Token.Type.LITERAL, "false", false),
            ),
        )
    }

    @Test
    fun `should support operators`() {
        val expression = "true + false - true + false++"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "true", true),
                Token(Token.Type.BINARY_OP, "+", "+"),
                Token(
                    Token.Type.LITERAL,
                    "false",
                    false,
                ),
                Token(Token.Type.BINARY_OP, "-", "-"),
                Token(Token.Type.LITERAL, "true", true),
                Token(Token.Type.BINARY_OP, "+", "+"),
                Token(
                    Token.Type.LITERAL,
                    "false",
                    false,
                ),
                Token(Token.Type.BINARY_OP, "+", "+"),
                Token(Token.Type.BINARY_OP, "+", "+"),
            ),
        )
    }

    @Test
    fun `should support operators with two characters`() {
        val expression = "true == false + true != false >= true > in false"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "true", true),
                Token(Token.Type.BINARY_OP, "==", "=="),
                Token(
                    Token.Type.LITERAL,
                    "false",
                    false,
                ),
                Token(Token.Type.BINARY_OP, "+", "+"),
                Token(Token.Type.LITERAL, "true", true),
                Token(Token.Type.BINARY_OP, "!=", "!="),
                Token(
                    Token.Type.LITERAL,
                    "false",
                    false,
                ),
                Token(Token.Type.BINARY_OP, ">=", ">="),
                Token(Token.Type.LITERAL, "true", true),
                Token(Token.Type.BINARY_OP, ">", ">"),
                Token(Token.Type.BINARY_OP, "in", "in"),
                Token(Token.Type.LITERAL, "false", false),
            ),
        )
    }

    @Test
    fun `should support numerics`() {
        val expression = "1234 == 782"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "1234", 1234),
                Token(Token.Type.BINARY_OP, "==", "=="),
                Token(Token.Type.LITERAL, "782", 782),
            ),
        )
    }

    @Test
    fun `should support negative numerics`() {
        val expression = "-7.6 + (-20 * -1)"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "-7.6", -7.6),
                Token(Token.Type.BINARY_OP, "+", "+"),
                Token(Token.Type.OPEN_PAREN, "(", "("),
                Token(Token.Type.LITERAL, "-20", -20),
                Token(Token.Type.BINARY_OP, "*", "*"),
                Token(Token.Type.LITERAL, "-1", -1),
                Token(Token.Type.CLOSE_PAREN, ")", ")"),
            ),
        )
    }

    @Test
    fun `should support floating point numerics`() {
        val expression = "1.337 != 2.42"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.LITERAL,
                    "1.337",
                    1.337,
                ),
                Token(Token.Type.BINARY_OP, "!=", "!="),
                Token(Token.Type.LITERAL, "2.42", 2.42),
            ),
        )
    }

    @Test
    fun `should support identifiers, numerics and operators`() {
        val expression = "person.age == 12 && (person.hasJob == true || person.onVacation != false)"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.IDENTIFIER,
                    "person",
                    "person",
                ),
                Token(Token.Type.DOT, ".", "."),
                Token(
                    Token.Type.IDENTIFIER,
                    "age",
                    "age",
                ),
                Token(Token.Type.BINARY_OP, "==", "=="),
                Token(Token.Type.LITERAL, "12", 12),
                Token(Token.Type.BINARY_OP, "&&", "&&"),
                Token(Token.Type.OPEN_PAREN, "(", "("),
                Token(
                    Token.Type.IDENTIFIER,
                    "person",
                    "person",
                ),
                Token(Token.Type.DOT, ".", "."),
                Token(
                    Token.Type.IDENTIFIER,
                    "hasJob",
                    "hasJob",
                ),
                Token(Token.Type.BINARY_OP, "==", "=="),
                Token(Token.Type.LITERAL, "true", true),
                Token(Token.Type.BINARY_OP, "||", "||"),
                Token(
                    Token.Type.IDENTIFIER,
                    "person",
                    "person",
                ),
                Token(Token.Type.DOT, ".", "."),
                Token(
                    Token.Type.IDENTIFIER,
                    "onVacation",
                    "onVacation",
                ),
                Token(Token.Type.BINARY_OP, "!=", "!="),
                Token(
                    Token.Type.LITERAL,
                    "false",
                    false,
                ),
                Token(Token.Type.CLOSE_PAREN, ")", ")"),
            ),
        )
    }

    @Test
    fun `tokenize math expression`() {
        val expression = "age * (3 - 1)"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.IDENTIFIER,
                    "age",
                    "age",
                ),
                Token(Token.Type.BINARY_OP, "*", "*"),
                Token(Token.Type.OPEN_PAREN, "(", "("),
                Token(Token.Type.LITERAL, "3", 3),
                Token(Token.Type.BINARY_OP, "-", "-"),
                Token(Token.Type.LITERAL, "1", 1),
                Token(Token.Type.CLOSE_PAREN, ")", ")"),
            ),
        )
    }

    @Test
    fun `should not split grammar elements out of transforms`() {
        val expression = "inString"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(
                    Token.Type.IDENTIFIER,
                    "inString",
                    "inString",
                ),
            ),
        )
    }

    @Test
    fun `should handle a complex mix of comments in single, multiline and value contexts`() {
        val expression = """
            6+x -  -17.55*y #end comment
            <= !foo.bar["baz\"foz"] # with space
            && b=="not a #comment" # is a comment
            # comment # 2nd comment
        """.trimIndent()

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "6", 6),
                Token(Token.Type.BINARY_OP, "+", "+"),
                Token(Token.Type.IDENTIFIER, "x", "x"),
                Token(Token.Type.BINARY_OP, "-", "-"),
                Token(
                    Token.Type.LITERAL,
                    "-17.55",
                    -17.55,
                ),
                Token(Token.Type.BINARY_OP, "*", "*"),
                Token(Token.Type.IDENTIFIER, "y", "y"),
                Token(Token.Type.BINARY_OP, "<=", "<="),
                Token(Token.Type.UNARY_OP, "!", "!"),
                Token(
                    Token.Type.IDENTIFIER,
                    "foo",
                    "foo",
                ),
                Token(Token.Type.DOT, ".", "."),
                Token(
                    Token.Type.IDENTIFIER,
                    "bar",
                    "bar",
                ),
                Token(Token.Type.OPEN_BRACKET, "[", "["),
                Token(
                    Token.Type.LITERAL,
                    "\"baz\\\"foz\"",
                    "baz\"foz",
                ),
                Token(
                    Token.Type.CLOSE_BRACKET,
                    "]",
                    "]",
                ),
                Token(Token.Type.BINARY_OP, "&&", "&&"),
                Token(Token.Type.IDENTIFIER, "b", "b"),
                Token(Token.Type.BINARY_OP, "==", "=="),
                Token(
                    Token.Type.LITERAL,
                    "\"not a #comment\"",
                    "not a #comment",
                ),
            ),
        )
    }

    @Test
    fun `should tokenize a full expression`() {
        val expression = """6+x -  -17.55*y<= !foo.bar["baz\\"foz"]"""

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "6", 6),
                Token(Token.Type.BINARY_OP, "+", "+"),
                Token(Token.Type.IDENTIFIER, "x", "x"),
                Token(Token.Type.BINARY_OP, "-", "-"),
                Token(Token.Type.LITERAL, "-17.55", -17.55),
                Token(Token.Type.BINARY_OP, "*", "*"),
                Token(Token.Type.IDENTIFIER, "y", "y"),
                Token(Token.Type.BINARY_OP, "<=", "<="),
                Token(Token.Type.UNARY_OP, "!", "!"),
                Token(Token.Type.IDENTIFIER, "foo", "foo"),
                Token(Token.Type.DOT, ".", "."),
                Token(Token.Type.IDENTIFIER, "bar", "bar"),
                Token(Token.Type.OPEN_BRACKET, "[", "["),
                Token(Token.Type.LITERAL, """"baz\\"foz"""", """baz\"foz"""),
                Token(Token.Type.CLOSE_BRACKET, "]", "]"),
            ),
        )
    }

    @Test
    fun `should consider minus to be negative appropriately`() {
        val expression = "-1?-2:-3"

        assertExpressionYieldsTokens(
            expression,
            listOf(
                Token(Token.Type.LITERAL, "-1", -1),
                Token(Token.Type.QUESTION, "?", "?"),
                Token(Token.Type.LITERAL, "-2", -2),
                Token(Token.Type.COLON, ":", ":"),
                Token(Token.Type.LITERAL, "-3", -3),
            ),
        )
    }

    private fun assertExpressionYieldsTokens(expression: String, tokens: List<Token>) {
        val actual = lexer.tokenize(expression)

        println(actual)

        assertEquals(tokens.size, actual.size)

        for (i in 0 until tokens.size) {
            assertEquals(tokens[i], actual[i])
        }
    }
}
