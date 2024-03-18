/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl

import mozilla.components.lib.jexl.evaluator.Evaluator
import mozilla.components.lib.jexl.evaluator.EvaluatorException
import mozilla.components.lib.jexl.evaluator.JexlContext
import mozilla.components.lib.jexl.evaluator.Transform
import mozilla.components.lib.jexl.grammar.Grammar
import mozilla.components.lib.jexl.lexer.Lexer
import mozilla.components.lib.jexl.lexer.LexerException
import mozilla.components.lib.jexl.parser.Parser
import mozilla.components.lib.jexl.parser.ParserException
import mozilla.components.lib.jexl.value.JexlUndefined
import mozilla.components.lib.jexl.value.JexlValue

class Jexl(
    private val grammar: Grammar = Grammar(),
) {
    private val lexer: Lexer = Lexer(grammar)
    private val transforms: MutableMap<String, Transform> = mutableMapOf()

    /**
     * Adds or replaces a transform function in this Jexl instance.
     *
     * @param name The name of the transform function, as it will be used within Jexl expressions.
     * @param transform The function to be executed when this transform is invoked. It will be
     *                  provided with two arguments:
     *                   - value: The value to be transformed
     *                   - arguments: The list of arguments for this transform.
     */
    fun addTransform(name: String, transform: Transform) {
        transforms[name] = transform
    }

    /**
     * Evaluates a Jexl string within an optional context.
     *
     * @param expression The Jexl expression to be evaluated.
     * @param context A mapping of variables to values, which will be made accessible to the Jexl
     *                expression when evaluating it.
     * @return The result of the evaluation.
     * @throws JexlException if lexing, parsing or evaluating the expression failed.
     */
    @Throws(JexlException::class)
    @Suppress("ThrowsCount")
    fun evaluate(expression: String, context: JexlContext = JexlContext()): JexlValue {
        val parser = Parser(grammar)
        val evaluator = Evaluator(context, grammar, transforms)

        try {
            val tokens = lexer.tokenize(expression)
            val astTree = parser.parse(tokens)
                ?: return JexlUndefined()

            return evaluator.evaluate(astTree)
        } catch (e: LexerException) {
            throw JexlException(e)
        } catch (e: ParserException) {
            throw JexlException(e)
        } catch (e: EvaluatorException) {
            throw JexlException(e)
        }
    }

    /**
     * Evaluates a Jexl string with an optional context to a Boolean result. Optionally a default
     * value can be provided that will be returned in the expression does not return a boolean
     * result.
     */
    fun evaluateBooleanExpression(
        expression: String,
        context: JexlContext = JexlContext(),
        defaultValue: Boolean? = null,
    ): Boolean {
        val result = try {
            evaluate(expression, context)
        } catch (e: EvaluatorException) {
            throw JexlException(e)
        }

        return try {
            result.toBoolean()
        } catch (e: EvaluatorException) {
            if (defaultValue != null) {
                return defaultValue
            } else {
                throw JexlException(e)
            }
        }
    }
}

/**
 * Generic exception thrown when evaluating an expression failed.
 */
class JexlException(
    cause: Exception? = null,
    message: String? = null,
) : Exception(message, cause)
