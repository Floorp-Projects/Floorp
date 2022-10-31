/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.grammar

import mozilla.components.lib.jexl.evaluator.EvaluatorException
import mozilla.components.lib.jexl.lexer.Token
import mozilla.components.lib.jexl.value.JexlArray
import mozilla.components.lib.jexl.value.JexlBoolean
import mozilla.components.lib.jexl.value.JexlDouble
import mozilla.components.lib.jexl.value.JexlInteger
import mozilla.components.lib.jexl.value.JexlString
import mozilla.components.lib.jexl.value.JexlValue
import kotlin.math.floor

/**
 * Grammar of the JEXL language.
 *
 * Note that changes here may require a change in the Lexer or Parser.
 */
@Suppress("MagicNumber") // Operator precedence uses numbers and I do not see the need for constants..
class Grammar {
    val elements: Map<String, GrammarElement> = mapOf(
        "." to GrammarElement(Token.Type.DOT),
        "[" to GrammarElement(Token.Type.OPEN_BRACKET),
        "]" to GrammarElement(Token.Type.CLOSE_BRACKET),
        "|" to GrammarElement(Token.Type.PIPE),
        "{" to GrammarElement(Token.Type.OPEN_CURL),
        "}" to GrammarElement(Token.Type.CLOSE_CURL),
        ":" to GrammarElement(Token.Type.COLON),
        "," to GrammarElement(Token.Type.COMMA),
        "(" to GrammarElement(Token.Type.OPEN_PAREN),
        ")" to GrammarElement(Token.Type.CLOSE_PAREN),
        "?" to GrammarElement(Token.Type.QUESTION),
        "+" to GrammarElement(
            Token.Type.BINARY_OP,
            30,
        ) { left, right -> left + right },
        "-" to GrammarElement(Token.Type.BINARY_OP, 30),
        "*" to GrammarElement(
            Token.Type.BINARY_OP,
            40,
        ) { left, right -> left * right },
        "/" to GrammarElement(Token.Type.BINARY_OP, 40) { left, right ->
            left / right
        },
        "//" to GrammarElement(
            Token.Type.BINARY_OP,
            40,
        ) { left, right ->
            when (val result = left.div(right)) {
                is JexlInteger -> result
                is JexlDouble -> JexlInteger(
                    floor(
                        result.value,
                    ).toInt(),
                )
                else -> throw EvaluatorException("Cannot floor type: " + result::class)
            }
        },
        "%" to GrammarElement(Token.Type.BINARY_OP, 50),
        "^" to GrammarElement(Token.Type.BINARY_OP, 50),
        "==" to GrammarElement(
            Token.Type.BINARY_OP,
            20,
        ) { left, right ->
            JexlBoolean(left == right)
        },
        "!=" to GrammarElement(
            Token.Type.BINARY_OP,
            20,
        ) { left, right ->
            JexlBoolean(left != right)
        },
        ">" to GrammarElement(
            Token.Type.BINARY_OP,
            20,
        ) { left, right ->
            JexlBoolean(left > right)
        },
        ">=" to GrammarElement(
            Token.Type.BINARY_OP,
            20,
        ) { left, right ->
            JexlBoolean(left >= right)
        },
        "<" to GrammarElement(
            Token.Type.BINARY_OP,
            20,
        ) { left, right ->
            JexlBoolean(left < right)
        },
        "<=" to GrammarElement(
            Token.Type.BINARY_OP,
            20,
        ) { left, right ->
            JexlBoolean(left <= right)
        },
        "&&" to GrammarElement(
            Token.Type.BINARY_OP,
            10,
        ) { left, right ->
            JexlBoolean(left.toBoolean() && right.toBoolean())
        },
        "||" to GrammarElement(
            Token.Type.BINARY_OP,
            10,
        ) { left, right ->
            JexlBoolean(left.toBoolean() || right.toBoolean())
        },
        "in" to GrammarElement(
            Token.Type.BINARY_OP,
            20,
        ) { left, right ->
            when {
                left is JexlString -> JexlBoolean(
                    right.toString().contains(left.value),
                )
                right is JexlArray -> JexlBoolean(
                    right.value.contains(left),
                )
                else -> throw EvaluatorException(
                    "Operator 'in' not applicable to " + left::class + " and " + right::class,
                )
            }
        },
        "!" to GrammarElement(
            Token.Type.UNARY_OP,
            Int.MAX_VALUE,
        ) { _, right ->
            JexlBoolean(!right.toBoolean())
        },
    )
}

data class GrammarElement(
    val type: Token.Type,
    val precedence: Int = 0,
    val evaluate: ((JexlValue, JexlValue) -> JexlValue)? = null,
)
