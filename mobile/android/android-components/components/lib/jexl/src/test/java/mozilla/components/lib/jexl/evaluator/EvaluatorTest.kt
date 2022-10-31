/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.evaluator

import mozilla.components.lib.jexl.ast.AstNode
import mozilla.components.lib.jexl.grammar.Grammar
import mozilla.components.lib.jexl.lexer.Lexer
import mozilla.components.lib.jexl.parser.Parser
import mozilla.components.lib.jexl.value.JexlArray
import mozilla.components.lib.jexl.value.JexlInteger
import mozilla.components.lib.jexl.value.JexlObject
import mozilla.components.lib.jexl.value.JexlString
import mozilla.components.lib.jexl.value.JexlUndefined
import mozilla.components.lib.jexl.value.JexlValue
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Ignore
import org.junit.Test

class EvaluatorTest {
    private lateinit var grammar: Grammar

    @Before
    fun setUp() {
        grammar = Grammar()
    }

    @Test
    fun `Should evaluate a literal`() {
        assertExpressionYieldsResult("42", 42)
        assertExpressionYieldsResult("2.0", 2.0)

        assertExpressionYieldsResult("true", true)
        assertExpressionYieldsResult("false", false)

        assertExpressionYieldsResult("\"hello world\"", "hello world")
        assertExpressionYieldsResult("'hello world'", "hello world")
    }

    @Test
    fun `Should evaluate an arithmetic expression`() {
        assertExpressionYieldsResult(
            "(2 + 3) * 4",
            20,
        )
    }

    @Test
    fun `Should evaluate a string concat`() {
        assertExpressionYieldsResult(
            """
                "Hello" + (4+4) + "Wo\"rld"
            """.trimIndent(),
            "Hello8Wo\"rld",
        )
    }

    @Test
    fun `Should evaluate a true comparison expression`() {
        assertExpressionYieldsResult(
            "2 > 1",
            true,
        )
    }

    @Test
    fun `Should evaluate a false comparison expression`() {
        assertExpressionYieldsResult(
            "2 <= 1",
            false,
        )
    }

    @Test
    fun `Should evaluate a complex expression`() {
        assertExpressionYieldsResult(
            "\"foo\" && 6 >= 6 && 0 + 1 && true",
            true,
        )
    }

    @Test
    fun `Should evaluate an identifier chain`() {
        val context = JexlContext(
            "foo" to JexlObject(
                "baz" to JexlObject(
                    "bar" to JexlString("tek"),
                ),
            ),
        )

        assertExpressionYieldsResult(
            "foo.baz.bar",
            "tek",
            context = context,
        )
    }

    @Test
    fun `Should apply transforms`() {
        val context = JexlContext(
            "foo" to JexlInteger(10),
        )

        assertExpressionYieldsResult(
            "foo|half + 3",
            8,
            context = context,
            transforms = mapOf(
                "half" to { value, _ ->
                    value.div(JexlInteger(2))
                },
            ),
        )
    }

    @Test
    fun `Should filter arrays`() {
        val context = JexlContext(
            "foo" to JexlObject(
                "bar" to JexlArray(
                    JexlObject("tek" to JexlString("hello")),
                    JexlObject("tek" to JexlString("baz")),
                    JexlObject("tok" to JexlString("baz")),
                ),
            ),
        )

        assertExpressionYieldsResult(
            "foo.bar[.tek == \"baz\"]",
            listOf(JexlObject("tek" to JexlString("baz"))),
            context = context,
        )
    }

    @Test
    fun `Should assume array index 0 when traversing`() {
        val context = JexlContext(
            "foo" to JexlObject(
                "bar" to JexlArray(
                    JexlObject(
                        "tek" to JexlObject(
                            "hello" to JexlString(
                                "world",
                            ),
                        ),
                    ),
                    JexlObject(
                        "tek" to JexlObject(
                            "hello" to JexlString(
                                "universe",
                            ),
                        ),
                    ),
                ),
            ),
        )

        assertExpressionYieldsResult(
            "foo.bar.tek.hello",
            "world",
            context = context,
        )
    }

    @Test
    fun `Should make array elements addressable by index`() {
        val context = JexlContext(
            "foo" to JexlObject(
                "bar" to JexlArray(
                    JexlObject("tek" to JexlString("tok")),
                    JexlObject("tek" to JexlString("baz")),
                    JexlObject("tek" to JexlString("foz")),
                ),
            ),
        )

        assertExpressionYieldsResult(
            "foo.bar[1].tek",
            "baz",
            context = context,
        )
    }

    @Test
    fun `Should allow filters to select object properties`() {
        val context = JexlContext(
            "foo" to JexlObject(
                "baz" to JexlObject(
                    "bar" to JexlString("tek"),
                ),
            ),
        )

        assertExpressionYieldsResult(
            "foo[\"ba\" + \"z\"].bar",
            "tek",
            context = context,
        )
    }

    @Test
    fun `Should allow simple filters on undefined objects`() {
        val context = JexlContext(
            "foo" to JexlObject(),
        )

        assertExpressionYieldsResult(
            "foo.bar[\"baz\"].tok",
            JexlUndefined(),
            context = context,
            unpack = false,
        )
    }

    @Test
    fun `Should allow complex filters on undefined objects`() {
        val context = JexlContext(
            "foo" to JexlObject(),
        )

        assertExpressionYieldsResult(
            "foo.bar[.size > 1].baz",
            JexlUndefined(),
            context = context,
            unpack = false,
        )
    }

    @Test(expected = EvaluatorException::class)
    fun `Should throw when transform does not exist`() {
        assertExpressionYieldsResult(
            "\"hello\"|world",
            "-- should throw",
        )
    }

    @Test
    fun `Should apply the DivFloor operator`() {
        assertExpressionYieldsResult(
            "7 // 2",
            3,
        )
    }

    @Test
    fun `Should evaluate an object literal`() {
        assertExpressionYieldsResult(
            "{foo: {bar: \"tek\"}}",
            JexlObject(
                "foo" to JexlObject(
                    "bar" to JexlString(
                        "tek",
                    ),
                ),
            ),
            unpack = false,
        )
    }

    @Test
    fun `Should evaluate an empty object literal`() {
        assertExpressionYieldsResult(
            "{}",
            emptyMap<String, JexlValue>(),
        )
    }

    @Test
    fun `Should evaluate a transform with multiple args`() {
        assertExpressionYieldsResult(
            """"foo"|concat("baz", "bar", "tek")""",
            "foo: bazbartek",
            transforms = mapOf(
                "concat" to { value, arguments ->
                    value + JexlString(": ") + JexlString(
                        arguments.joinToString(""),
                    )
                },
            ),
        )
    }

    @Test
    fun `Should evaluate dot notation for object literals`() {
        assertExpressionYieldsResult(
            "{foo: \"bar\"}.foo",
            "bar",
        )
    }

    @Test
    @Ignore("JavaScript properties are not implemented yet")
    fun `Should allow access to literal properties`() {
        assertExpressionYieldsResult(
            "\"foo\".length",
            3,
        )
    }

    @Test
    fun `Should evaluate array literals`() {
        assertExpressionYieldsResult(
            "[\"foo\", 1+2]",
            listOf(
                JexlString("foo"),
                JexlInteger(3),
            ),
        )
    }

    @Test
    fun `Should apply the 'in' operator to strings`() {
        assertExpressionYieldsResult(""""bar" in "foobartek"""", true)
        assertExpressionYieldsResult(""""baz" in "foobartek"""", false)
    }

    @Test
    fun `Should apply the 'in' operator to arrays`() {
        assertExpressionYieldsResult(""""bar" in ["foo","bar","tek"]""", true)
        assertExpressionYieldsResult(""""baz" in ["foo","bar","tek"]""", false)
    }

    @Test
    fun `Should evaluate a conditional expression`() {
        assertExpressionYieldsResult("\"foo\" ? 1 : 2", 1)
        assertExpressionYieldsResult("\"\" ? 1 : 2", 2)
    }

    @Test
    fun `Should allow missing consequent in ternary`() {
        assertExpressionYieldsResult(""""foo" ?: "bar"""", "foo")
    }

    @Test
    @Ignore("JavaScript properties are not implemented yet")
    fun `Does not treat falsey properties as undefined`() {
        assertExpressionYieldsResult("\"\".length", 0)
    }

    @Test
    fun `Should handle an expression with arbitrary whitespace`() {
        assertExpressionYieldsResult("(\t2\n+\n3) *\n4\n\r\n", 20)
    }

    private fun assertExpressionYieldsResult(
        expression: String,
        result: Any,
        context: JexlContext = JexlContext(),
        transforms: Map<String, Transform> = emptyMap(),
        unpack: Boolean = true,
    ) {
        val tree = toTree(expression)

        println(tree)

        val evaluator = Evaluator(context, grammar, transforms)
        val actual = evaluator.evaluate(tree)

        assertEquals(result, if (unpack) actual.value else actual)
    }

    private fun toTree(
        expression: String,
        grammar: Grammar = Grammar(),
    ): AstNode {
        val lexer = Lexer(grammar)
        val parser = Parser(grammar)

        return parser.parse(lexer.tokenize(expression))
            ?: throw AssertionError("Expression yielded null AST tree")
    }
}
