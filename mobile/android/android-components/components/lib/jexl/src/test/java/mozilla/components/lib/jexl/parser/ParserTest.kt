/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.parser

import mozilla.components.lib.jexl.ast.AstNode
import mozilla.components.lib.jexl.ast.AstType
import mozilla.components.lib.jexl.grammar.Grammar
import mozilla.components.lib.jexl.lexer.Lexer
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class ParserTest {
    @Test
    fun `Should parse literal`() {
        val expression = "42"

        assertExpressionYieldsTree(expression,
            AstNode(AstType.LITERAL, 42)
        )
    }

    @Test
    fun `Should parse math expression`() {
        val expression = "42 + 23"

        assertExpressionYieldsTree(expression,
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "+",
                left = AstNode(
                    AstType.LITERAL,
                    42
                ),
                right = AstNode(
                    AstType.LITERAL,
                    23
                )
            )
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

        assertExpressionYieldsTree(expression,
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = ">",
                left = AstNode(
                    AstType.IDENTIFIER,
                    "age"
                ),
                right = AstNode(
                    AstType.LITERAL,
                    21
                )
            )
        )
    }

    @Test
    fun `Should parse expression with sub expression`() {
        val expression = "(age + 5) > 42"

        assertExpressionYieldsTree(expression,
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = ">",
                left = AstNode(
                    AstType.BINARY_EXPRESSION,
                    operator = "+",
                    left = AstNode(
                        AstType.IDENTIFIER,
                        "age"
                    ),
                    right = AstNode(
                        AstType.LITERAL,
                        5
                    )
                ),
                right = AstNode(
                    AstType.LITERAL,
                    42
                )
            )
        )
    }

    @Test
    fun `Should parse expression following operator precedence`() {
        assertExpressionYieldsTree("5 + 7 * 2",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "+",
                left = AstNode(
                    AstType.LITERAL,
                    5
                ),
                right = AstNode(
                    AstType.BINARY_EXPRESSION,
                    operator = "*",
                    left = AstNode(
                        AstType.LITERAL,
                        7
                    ),
                    right = AstNode(
                        AstType.LITERAL,
                        2
                    )
                )
            )
        )

        assertExpressionYieldsTree("5 * 7 + 2",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "+",
                left = AstNode(
                    AstType.BINARY_EXPRESSION,
                    operator = "*",
                    left = AstNode(
                        AstType.LITERAL,
                        5
                    ),
                    right = AstNode(
                        AstType.LITERAL,
                        7
                    )
                ),
                right = AstNode(
                    AstType.LITERAL,
                    2
                )
            )
        )
    }

    @Test
    fun `Should handle encapsulation of subtree`() {
        assertExpressionYieldsTree("2+3*4==5/6-7",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "==",
                left = AstNode(
                    AstType.BINARY_EXPRESSION,
                    operator = "+",
                    left = AstNode(
                        AstType.LITERAL,
                        2
                    ),
                    right = AstNode(
                        AstType.BINARY_EXPRESSION,
                        operator = "*",
                        left = AstNode(
                            AstType.LITERAL,
                            3
                        ),
                        right = AstNode(
                            AstType.LITERAL,
                            4
                        )
                    )
                ),
                right = AstNode(
                    AstType.BINARY_EXPRESSION,
                    operator = "-",
                    left = AstNode(
                        AstType.BINARY_EXPRESSION,
                        operator = "/",
                        left = AstNode(
                            AstType.LITERAL,
                            5
                        ),
                        right = AstNode(
                            AstType.LITERAL,
                            6
                        )
                    ),
                    right = AstNode(
                        AstType.LITERAL,
                        7
                    )
                )
            )
        )
    }

    @Test
    fun `Should handle a unary operator`() {
        assertExpressionYieldsTree("1*!!true-2",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "-",
                left = AstNode(
                    AstType.BINARY_EXPRESSION,
                    operator = "*",
                    left = AstNode(
                        AstType.LITERAL,
                        1
                    ),
                    right = AstNode(
                        AstType.UNARY_EXPRESSION,
                        operator = "!",
                        right = AstNode(
                            AstType.UNARY_EXPRESSION,
                            operator = "!",
                            right = AstNode(
                                AstType.LITERAL,
                                true
                            )
                        )
                    )
                ),
                right = AstNode(
                    AstType.LITERAL,
                    2
                )
            )
        )
    }

    @Test
    fun `Should handle nested subexpressions`() {
        assertExpressionYieldsTree("(4*(2+3))/5",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "/",
                left = AstNode(
                    AstType.BINARY_EXPRESSION,
                    operator = "*",
                    left = AstNode(
                        AstType.LITERAL,
                        4
                    ),
                    right = AstNode(
                        AstType.BINARY_EXPRESSION,
                        operator = "+",
                        left = AstNode(
                            AstType.LITERAL,
                            2
                        ),
                        right = AstNode(
                            AstType.LITERAL,
                            3
                        )
                    )
                ),
                right = AstNode(
                    AstType.LITERAL,
                    5
                )
            )
        )
    }

    @Test
    fun `Should handle whitespace in an expression`() {
        assertExpressionYieldsTree("\t2\r\n+\n\r3\n\n",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "+",
                left = AstNode(
                    AstType.LITERAL,
                    2
                ),
                right = AstNode(
                    AstType.LITERAL,
                    3
                )
            )
        )
    }

    @Test
    fun `Should handle object literals`() {
        assertExpressionYieldsTree("{foo: \"bar\", tek: 1+2}",
            AstNode(
                AstType.OBJECT_LITERAL,
                value = mapOf(
                    "foo" to AstNode(
                        AstType.LITERAL,
                        "bar"
                    ),
                    "tek" to AstNode(
                        AstType.BINARY_EXPRESSION,
                        operator = "+",
                        left = AstNode(
                            AstType.LITERAL,
                            1
                        ),
                        right = AstNode(
                            AstType.LITERAL,
                            2
                        )
                    )
                )
            )
        )
    }

    @Test
    fun `Should handle nested object literals`() {
        assertExpressionYieldsTree("{foo: {bar: \"tek\"}}",
            AstNode(
                AstType.OBJECT_LITERAL,
                value = mapOf(
                    "foo" to AstNode(
                        AstType.OBJECT_LITERAL,
                        value = mapOf(
                            "bar" to AstNode(
                                AstType.LITERAL,
                                "tek"
                            )
                        )
                    )
                )
            )
        )
    }

    @Test
    fun `Should handle empty object literals`() {
        assertExpressionYieldsTree("{}",
            AstNode(
                AstType.OBJECT_LITERAL,
                value = emptyMap<String, AstNode>()
            )
        )
    }

    @Test
    fun `Should handle array literals`() {
        assertExpressionYieldsTree("[\"foo\", 1+2]",
            AstNode(
                AstType.ARRAY_LITERAL, listOf(
                    AstNode(
                        AstType.LITERAL,
                        "foo"
                    ),
                    AstNode(
                        AstType.BINARY_EXPRESSION,
                        operator = "+",
                        left = AstNode(
                            AstType.LITERAL,
                            1
                        ),
                        right = AstNode(
                            AstType.LITERAL,
                            2
                        )
                    )
                )
            )
        )
    }

    @Test
    fun `Should handle nested array literals`() {
        assertExpressionYieldsTree("[\"foo\", [\"bar\", \"tek\"]]",
            AstNode(
                AstType.ARRAY_LITERAL, listOf(
                    AstNode(
                        AstType.LITERAL,
                        "foo"
                    ),
                    AstNode(
                        AstType.ARRAY_LITERAL, listOf(
                            AstNode(
                                AstType.LITERAL,
                                "bar"
                            ),
                            AstNode(
                                AstType.LITERAL,
                                "tek"
                            )
                        )
                    )
                )
            )
        )
    }

    @Test
    fun `Should handle empty array literals`() {
        assertExpressionYieldsTree("[]",
            AstNode(
                AstType.ARRAY_LITERAL,
                emptyList<AstNode>()
            )
        )
    }

    @Test
    fun `Should chain traversed identifiers`() {
        assertExpressionYieldsTree("foo.bar.baz + 1",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "+",
                left = AstNode(
                    AstType.IDENTIFIER, "baz",
                    from = AstNode(
                        AstType.IDENTIFIER, "bar",
                        from = AstNode(
                            AstType.IDENTIFIER,
                            "foo"
                        )
                    )
                ),
                right = AstNode(
                    AstType.LITERAL,
                    1
                )
            )
        )
    }

    @Test
    fun `Should apply transforms and arguments`() {
        assertExpressionYieldsTree("foo|tr1|tr2.baz|tr3({bar:\"tek\"})",
            AstNode(
                AstType.TRANSFORM,
                name = "tr3",
                arguments = mutableListOf(
                    AstNode(
                        AstType.OBJECT_LITERAL,
                        value = mapOf(
                            "bar" to AstNode(
                                AstType.LITERAL,
                                "tek"
                            )
                        )
                    )
                ),
                subject = AstNode(
                    AstType.IDENTIFIER,
                    value = "baz",
                    from = AstNode(
                        AstType.TRANSFORM,
                        name = "tr2",
                        subject = AstNode(
                            AstType.TRANSFORM,
                            name = "tr1",
                            subject = AstNode(
                                AstType.IDENTIFIER,
                                "foo"
                            )
                        )
                    )
                )
            )
        )
    }

    @Test
    fun `Should handle multiple arguments in transforms`() {
        assertExpressionYieldsTree("foo|bar(\"tek\", 5, true)",
            AstNode(
                AstType.TRANSFORM,
                name = "bar",
                subject = AstNode(
                    AstType.IDENTIFIER,
                    "foo"
                ),
                arguments = mutableListOf(
                    AstNode(
                        AstType.LITERAL,
                        "tek"
                    ),
                    AstNode(AstType.LITERAL, 5),
                    AstNode(AstType.LITERAL, true)
                )
            )
        )
    }

    @Test
    fun `Should apply filters to identifiers`() {
        assertExpressionYieldsTree("foo[1][.bar[0]==\"tek\"].baz",
            AstNode(
                AstType.IDENTIFIER, "baz",
                from = AstNode(
                    AstType.FILTER_EXPRESSION,
                    relative = true,
                    expression = AstNode(
                        AstType.BINARY_EXPRESSION,
                        operator = "==",
                        left = AstNode(
                            AstType.FILTER_EXPRESSION,
                            relative = false,
                            expression = AstNode(
                                AstType.LITERAL,
                                0
                            ),
                            subject = AstNode(
                                AstType.IDENTIFIER,
                                value = "bar",
                                relative = true
                            )
                        ),
                        right = AstNode(
                            AstType.LITERAL,
                            "tek"
                        )
                    ),
                    subject = AstNode(
                        AstType.FILTER_EXPRESSION,
                        relative = false,
                        expression = AstNode(
                            AstType.LITERAL,
                            1
                        ),
                        subject = AstNode(
                            AstType.IDENTIFIER,
                            "foo"
                        )
                    )
                )
            )
        )
    }

    @Test
    fun `Should allow dot notation for all operands`() {
        assertExpressionYieldsTree("\"foo\".length + {foo: \"bar\"}.foo",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "+",
                left = AstNode(
                    AstType.IDENTIFIER, "length",
                    from = AstNode(
                        AstType.LITERAL,
                        "foo"
                    )
                ),
                right = AstNode(
                    AstType.IDENTIFIER, "foo",
                    from = AstNode(
                        AstType.OBJECT_LITERAL, mutableMapOf(
                            "foo" to AstNode(
                                AstType.LITERAL,
                                "bar"
                            )
                        )
                    )
                )
            )
        )
    }

    @Test
    fun `Should allow dot notation on subexpressions`() {
        assertExpressionYieldsTree("(\"foo\" + \"bar\").length",
            AstNode(
                AstType.IDENTIFIER, "length",
                from = AstNode(
                    AstType.BINARY_EXPRESSION,
                    operator = "+",
                    left = AstNode(
                        AstType.LITERAL,
                        "foo"
                    ),
                    right = AstNode(
                        AstType.LITERAL,
                        "bar"
                    )
                )
            )
        )
    }

    @Test
    fun `Should allow dot notation on arrays`() {
        assertExpressionYieldsTree("[\"foo\", \"bar\"].length",
            AstNode(
                AstType.IDENTIFIER, "length",
                from = AstNode(
                    AstType.ARRAY_LITERAL,
                    value = listOf(
                        AstNode(
                            AstType.LITERAL,
                            "foo"
                        ),
                        AstNode(
                            AstType.LITERAL,
                            "bar"
                        )
                    )
                )
            )
        )
    }

    @Test
    fun `Should handle a ternary expression`() {
        assertExpressionYieldsTree("foo ? 1 : 0",
            AstNode(
                AstType.CONDITIONAL_EXPRESSION,
                test = AstNode(
                    AstType.IDENTIFIER,
                    "foo"
                ),
                consequent = AstNode(
                    AstType.LITERAL,
                    1
                ),
                alternate = AstNode(
                    AstType.LITERAL,
                    0
                )
            )
        )
    }

    @Test
    fun `Should handle nested and grouped ternary expressions`() {
        assertExpressionYieldsTree("foo ? (bar ? 1 : 2) : 3",
            AstNode(
                AstType.CONDITIONAL_EXPRESSION,
                test = AstNode(
                    AstType.IDENTIFIER,
                    "foo"
                ),
                consequent = AstNode(
                    AstType.CONDITIONAL_EXPRESSION,
                    test = AstNode(
                        AstType.LITERAL,
                        "bar"
                    ),
                    consequent = AstNode(
                        AstType.LITERAL,
                        1
                    ),
                    alternate = AstNode(
                        AstType.LITERAL,
                        2
                    )
                ),
                alternate = AstNode(
                    AstType.LITERAL,
                    3
                )
            )
        )
    }

    @Test
    fun `Should handle nested, non-grouped ternary expressions`() {
        assertExpressionYieldsTree("foo ? bar ? 1 : 2 : 3",
            AstNode(
                AstType.CONDITIONAL_EXPRESSION,
                test = AstNode(
                    AstType.IDENTIFIER,
                    "foo"
                ),
                consequent = AstNode(
                    AstType.CONDITIONAL_EXPRESSION,
                    test = AstNode(
                        AstType.IDENTIFIER, "bar",
                        consequent = AstNode(
                            AstType.LITERAL,
                            1
                        ),
                        alternate = AstNode(
                            AstType.LITERAL,
                            2
                        )
                    )
                ),
                alternate = AstNode(
                    AstType.LITERAL,
                    3
                )
            )
        )
    }

    @Test
    fun `Should handle ternary expression with objects`() {
        assertExpressionYieldsTree("foo ? {bar: \"tek\"} : \"baz\"",
            AstNode(
                AstType.CONDITIONAL_EXPRESSION,
                test = AstNode(
                    AstType.IDENTIFIER,
                    "foo"
                ),
                consequent = AstNode(
                    AstType.OBJECT_LITERAL, mutableMapOf(
                        "bar" to AstNode(
                            AstType.LITERAL,
                            "tek"
                        )
                    ),
                    alternate = AstNode(
                        AstType.LITERAL,
                        "bar"
                    )
                )
            )
        )
    }

    @Test
    fun `Should correctly balance a binary op between complex identifiers`() {
        assertExpressionYieldsTree("a.b == c.d",
            AstNode(
                AstType.BINARY_EXPRESSION,
                operator = "==",
                left = AstNode(
                    AstType.IDENTIFIER, "b",
                    from = AstNode(
                        AstType.IDENTIFIER,
                        "a"
                    )
                ),
                right = AstNode(
                    AstType.IDENTIFIER, "d",
                    from = AstNode(
                        AstType.IDENTIFIER,
                        "c"
                    )
                )
            )
        )
    }

    private fun assertExpressionYieldsTree(expression: String, tree: AstNode?) {
        val actual = parse(expression)

        if (tree != null) {
            assertNotNull(actual)
        }

        actual?.print()

        assertEquals(tree, actual)
    }

    private fun parse(expression: String): AstNode? {
        val grammar = Grammar()
        val lexer = Lexer(grammar)
        val parser = Parser(grammar)

        return parser.parse(lexer.tokenize(expression))
    }
}
