/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.parser

import mozilla.components.lib.jexl.ast.ArrayLiteral
import mozilla.components.lib.jexl.ast.AstNode
import mozilla.components.lib.jexl.ast.BinaryExpression
import mozilla.components.lib.jexl.ast.ConditionalExpression
import mozilla.components.lib.jexl.ast.FilterExpression
import mozilla.components.lib.jexl.ast.Identifier
import mozilla.components.lib.jexl.ast.Literal
import mozilla.components.lib.jexl.ast.ObjectLiteral
import mozilla.components.lib.jexl.ast.Transformation
import mozilla.components.lib.jexl.ast.UnaryExpression
import mozilla.components.lib.jexl.grammar.Grammar
import mozilla.components.lib.jexl.lexer.Lexer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class ParserTest {

    @Test
    fun `Should parse literal`() {
        val expression = "42"

        assertExpressionYieldsTree(
            expression,
            Literal(42),
        )
    }

    @Test
    fun `Should parse math expression`() {
        val expression = "42 + 23"

        assertExpressionYieldsTree(
            expression,
            BinaryExpression(
                left = Literal(42),
                right = Literal(23),
                operator = "+",
            ),
        )
    }

    @Test(expected = ParserException::class)
    fun `Should throw on incomplete expression`() {
        val expression = "42 +"
        parse(expression)
    }

    @Test
    fun `Should parse expression with identifier`() {
        val expression = "age > 21"

        assertExpressionYieldsTree(
            expression,
            BinaryExpression(
                operator = ">",
                left = Identifier("age"),
                right = Literal(21),
            ),
        )
    }

    @Test
    fun `Should parse expression with sub expression`() {
        val expression = "(age + 5) > 42"

        assertExpressionYieldsTree(
            expression,
            BinaryExpression(
                operator = ">",
                left = BinaryExpression(
                    operator = "+",
                    left = Identifier("age"),
                    right = Literal(5),
                ),
                right = Literal(42),
            ),
        )
    }

    @Test
    fun `Should parse expression following operator precedence`() {
        assertExpressionYieldsTree(
            "5 + 7 * 2",
            BinaryExpression(
                operator = "+",
                left = Literal(5),
                right = BinaryExpression(
                    operator = "*",
                    left = Literal(7),
                    right = Literal(2),
                ),
            ),
        )

        assertExpressionYieldsTree(
            "5 * 7 + 2",
            BinaryExpression(
                operator = "+",
                left = BinaryExpression(
                    operator = "*",
                    left = Literal(5),
                    right = Literal(7),
                ),
                right = Literal(2),
            ),
        )
    }

    @Test
    fun `Should handle encapsulation of subtree`() {
        assertExpressionYieldsTree(
            "2+3*4==5/6-7",
            BinaryExpression(
                operator = "==",
                left = BinaryExpression(
                    operator = "+",
                    left = Literal(2),
                    right = BinaryExpression(
                        operator = "*",
                        left = Literal(3),
                        right = Literal(4),
                    ),
                ),
                right = BinaryExpression(
                    operator = "-",
                    left = BinaryExpression(
                        operator = "/",
                        left = Literal(5),
                        right = Literal(6),
                    ),
                    right = Literal(7),
                ),
            ),
        )
    }

    @Test
    fun `Should handle a unary operator`() {
        assertExpressionYieldsTree(
            "1*!!true-2",
            BinaryExpression(
                operator = "-",
                left = BinaryExpression(
                    operator = "*",
                    left = Literal(1),
                    right = UnaryExpression(
                        operator = "!",
                        right = UnaryExpression(
                            operator = "!",
                            right = Literal(true),
                        ),
                    ),
                ),
                right = Literal(2),
            ),
        )
    }

    @Test
    fun `Should handle nested subexpressions`() {
        assertExpressionYieldsTree(
            "(4*(2+3))/5",
            BinaryExpression(
                operator = "/",
                left = BinaryExpression(
                    operator = "*",
                    left = Literal(4),
                    right = BinaryExpression(
                        operator = "+",
                        left = Literal(2),
                        right = Literal(3),
                    ),
                ),
                right = Literal(5),
            ),
        )
    }

    @Test
    fun `Should handle whitespace in an expression`() {
        assertExpressionYieldsTree(
            "\t2\r\n+\n\r3\n\n",
            BinaryExpression(
                operator = "+",
                left = Literal(2),
                right = Literal(3),
            ),
        )
    }

    @Test
    fun `Should handle object literals`() {
        assertExpressionYieldsTree(
            "{foo: \"bar\", tek: 1+2}",
            ObjectLiteral(
                "foo" to Literal("bar"),
                "tek" to BinaryExpression(
                    operator = "+",
                    left = Literal(1),
                    right = Literal(2),
                ),
            ),
        )
    }

    @Test
    fun `Should handle nested object literals`() {
        assertExpressionYieldsTree(
            """{
              foo: {
                bar: "tek",
                baz: 42
              }
            }""",
            ObjectLiteral(
                "foo" to ObjectLiteral(
                    "bar" to Literal("tek"),
                    "baz" to Literal(42),
                ),
            ),
        )
    }

    @Test
    fun `Should handle empty object literals`() {
        assertExpressionYieldsTree(
            "{}",
            ObjectLiteral(),
        )
    }

    @Test
    fun `Should handle array literals`() {
        assertExpressionYieldsTree(
            "[\"foo\", 1+2]",
            ArrayLiteral(
                Literal("foo"),
                BinaryExpression(
                    operator = "+",
                    left = Literal(1),
                    right = Literal(2),
                ),
            ),
        )
    }

    @Test
    fun `Should handle nested array literals`() {
        assertExpressionYieldsTree(
            "[\"foo\", [\"bar\", \"tek\"]]",
            ArrayLiteral(
                Literal("foo"),
                ArrayLiteral(
                    Literal("bar"),
                    Literal("tek"),
                ),
            ),
        )
    }

    @Test
    fun `Should handle empty array literals`() {
        assertExpressionYieldsTree(
            "[]",
            ArrayLiteral(),
        )
    }

    @Test
    fun `Should chain traversed identifiers`() {
        assertExpressionYieldsTree(
            "foo.bar.baz + 1",
            BinaryExpression(
                operator = "+",
                left = Identifier(
                    "baz",
                    from = Identifier(
                        "bar",
                        from = Identifier("foo"),
                    ),
                ),
                right = Literal(1),
            ),
        )
    }

    @Test
    fun `Should apply transforms and arguments`() {
        assertExpressionYieldsTree(
            "foo|tr1|tr2.baz|tr3({bar:\"tek\"})",
            Transformation(
                name = "tr3",
                arguments = mutableListOf(
                    ObjectLiteral(
                        "bar" to Literal("tek"),
                    ),
                ),
                subject = Identifier(
                    value = "baz",
                    from = Transformation(
                        name = "tr2",
                        subject = Transformation(
                            name = "tr1",
                            subject = Identifier("foo"),
                        ),
                    ),
                ),
            ),
        )
    }

    @Test
    fun `Should handle multiple arguments in transforms`() {
        assertExpressionYieldsTree(
            "foo|bar(\"tek\", 5, true)",
            Transformation(
                name = "bar",
                subject = Identifier("foo"),
                arguments = mutableListOf(
                    Literal("tek"),
                    Literal(5),
                    Literal(true),
                ),
            ),
        )
    }

    @Test
    fun `Should apply filters to identifiers`() {
        assertExpressionYieldsTree(
            """foo[1][.bar[0] == "tek"].baz""",
            Identifier(
                "baz",
                from = FilterExpression(
                    relative = true,
                    expression = BinaryExpression(
                        operator = "==",
                        left = FilterExpression(
                            relative = false,
                            expression = Literal(0),
                            subject = Identifier(
                                value = "bar",
                                relative = true,
                            ),
                        ),
                        right = Literal("tek"),
                    ),
                    subject = FilterExpression(
                        relative = false,
                        expression = Literal(1),
                        subject = Identifier("foo"),
                    ),
                ),
            ),
        )
    }

    @Test
    fun `Should allow dot notation for all operands`() {
        assertExpressionYieldsTree(
            "\"foo\".length + {foo: \"bar\"}.foo",
            BinaryExpression(
                operator = "+",
                left = Identifier("length", from = Literal("foo")),
                right = Identifier(
                    "foo",
                    from = ObjectLiteral(
                        "foo" to Literal("bar"),
                    ),
                ),
            ),
        )
    }

    @Test
    fun `Should allow dot notation on subexpressions`() {
        assertExpressionYieldsTree(
            "(\"foo\" + \"bar\").length",
            Identifier(
                "length",
                from = BinaryExpression(
                    operator = "+",
                    left = Literal("foo"),
                    right = Literal("bar"),
                ),
            ),
        )
    }

    @Test
    fun `Should allow dot notation on arrays`() {
        assertExpressionYieldsTree(
            "[\"foo\", \"bar\"].length",
            Identifier(
                "length",
                from = ArrayLiteral(
                    Literal("foo"),
                    Literal("bar"),
                ),
            ),
        )
    }

    @Test
    fun `Should handle a ternary expression`() {
        assertExpressionYieldsTree(
            "foo ? 1 : 0",
            ConditionalExpression(
                test = Identifier("foo"),
                consequent = Literal(1),
                alternate = Literal(0),
            ),
        )
    }

    @Test
    fun `Should handle nested and grouped ternary expressions`() {
        assertExpressionYieldsTree(
            "foo ? (bar ? 1 : 2) : 3",
            ConditionalExpression(
                test = Identifier("foo"),
                consequent = ConditionalExpression(
                    test = Identifier("bar"),
                    consequent = Literal(1),
                    alternate = Literal(2),
                ),
                alternate = Literal(3),
            ),
        )
    }

    @Test
    fun `Should handle nested, non-grouped ternary expressions`() {
        assertExpressionYieldsTree(
            "foo ? bar ? 1 : 2 : 3",
            ConditionalExpression(
                test = Identifier("foo"),
                consequent = ConditionalExpression(
                    test = Identifier("bar"),
                    consequent = Literal(1),
                    alternate = Literal(2),
                ),
                alternate = Literal(3),
            ),
        )
    }

    @Test
    fun `Should handle ternary expression with objects`() {
        assertExpressionYieldsTree(
            "foo ? {bar: \"tek\"} : \"baz\"",
            ConditionalExpression(
                test = Identifier("foo"),
                consequent = ObjectLiteral(
                    "bar" to Literal("tek"),
                ),
                alternate = Literal("baz"),
            ),
        )
    }

    @Test
    fun `Should correctly balance a binary op between complex identifiers`() {
        assertExpressionYieldsTree(
            "a.b == c.d",
            BinaryExpression(
                operator = "==",
                left = Identifier(
                    value = "b",
                    from = Identifier("a"),
                ),
                right = Identifier(
                    value = "d",
                    from = Identifier("c"),
                ),
            ),
        )
    }

    private fun assertExpressionYieldsTree(expression: String, tree: AstNode?) {
        val actual = parse(expression)

        if (tree != null) {
            assertNotNull(actual)
        }

        println(actual)

        assertEquals(tree, actual)
    }

    private fun parse(expression: String): AstNode? {
        val grammar = Grammar()
        val lexer = Lexer(grammar)
        val parser = Parser(grammar)

        return parser.parse(lexer.tokenize(expression))
    }
}
