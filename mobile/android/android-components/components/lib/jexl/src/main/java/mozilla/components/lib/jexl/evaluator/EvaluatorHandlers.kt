/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.evaluator

import mozilla.components.lib.jexl.JexlException
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
import mozilla.components.lib.jexl.value.JexlArray
import mozilla.components.lib.jexl.value.JexlBoolean
import mozilla.components.lib.jexl.value.JexlDouble
import mozilla.components.lib.jexl.value.JexlInteger
import mozilla.components.lib.jexl.value.JexlObject
import mozilla.components.lib.jexl.value.JexlString
import mozilla.components.lib.jexl.value.JexlUndefined
import mozilla.components.lib.jexl.value.JexlValue

/**
 * A mapping of AST node type to a function that can evaluate this type of node.
 *
 * This mapping could be moved inside [Evaluator].
 */
internal object EvaluatorHandlers {

    internal fun evaluateWith(evaluator: Evaluator, node: AstNode): JexlValue = when (node) {
        is Literal -> evaluateLiteral(node)
        is BinaryExpression -> evaluateBinaryExpression(evaluator, node)
        is Identifier -> evaluateIdentifier(evaluator, node)
        is ObjectLiteral -> evaluateObjectLiteral(evaluator, node)
        is ArrayLiteral -> evaluateArrayLiteral(evaluator, node)
        is ConditionalExpression -> evaluateConditionalExpression(evaluator, node)
        is Transformation -> evaluateTransformation(evaluator, node)
        is FilterExpression -> evaluateFilterExpression(evaluator, node)
        is UnaryExpression -> throw JexlException(
            message = "Unary expression evaluation can't be validated",
        )
    }

    private fun evaluateLiteral(node: Literal): JexlValue = when (val value = node.value) {
        is String -> JexlString(value)
        is Double -> JexlDouble(value)
        is Int -> JexlInteger(value)
        is Boolean -> JexlBoolean(value)
        else -> throw EvaluatorException("Unknown value type: ${value!!::class}")
    }

    private fun evaluateBinaryExpression(evaluator: Evaluator, node: BinaryExpression): JexlValue {
        val left = evaluator.evaluate(node.left!!)
        val right = evaluator.evaluate(node.right!!)
        val operator = evaluator.grammar.elements[node.operator!!]

        return operator!!.evaluate?.invoke(left, right)
            ?: throw EvaluatorException("Can't evaluate _operator: ${node.operator}")
    }

    private fun evaluateIdentifier(evaluator: Evaluator, node: Identifier): JexlValue =
        if (node.from != null) {
            evaluateIdentifierWithScope(evaluator, node)
        } else {
            evaluateIdentifierWithoutScope(evaluator, node)
        }

    private fun evaluateIdentifierWithScope(evaluator: Evaluator, node: Identifier): JexlValue =
        when (val subContext = evaluator.evaluate(node.from!!)) {
            is JexlArray -> {
                val obj = subContext.value[0]

                when (obj) {
                    is JexlUndefined -> obj

                    is JexlObject -> obj.value[node.value.toString()]
                        ?: throw EvaluatorException("${node.value} is undefined")

                    else -> throw EvaluatorException("$obj is not an object")
                }
            }

            is JexlObject -> subContext.value[node.value.toString()]
                ?: JexlUndefined()

            else -> JexlUndefined()
        }

    private fun evaluateIdentifierWithoutScope(evaluator: Evaluator, node: Identifier): JexlValue =
        if (node.relative) {
            evaluator.relativeContext.value[(node.value.toString())]
                ?: JexlUndefined()
        } else {
            evaluator.context.get(node.value.toString())
        }

    private fun evaluateObjectLiteral(evaluator: Evaluator, node: ObjectLiteral): JexlValue {
        val properties = evaluator.evaluateObject(node.properties)
        return JexlObject(properties)
    }

    private fun evaluateArrayLiteral(evaluator: Evaluator, node: ArrayLiteral): JexlValue {
        @Suppress("UNCHECKED_CAST")
        val values = evaluator.evaluateArray(node.values as List<AstNode>)
        return JexlArray(values)
    }

    private fun evaluateConditionalExpression(evaluator: Evaluator, node: ConditionalExpression): JexlValue {
        val result = evaluator.evaluate(node.test!!)

        return if (result.toBoolean()) {
            if (node.consequent != null) {
                evaluator.evaluate(node.consequent!!)
            } else {
                result
            }
        } else {
            evaluator.evaluate(node.alternate!!)
        }
    }

    private fun evaluateTransformation(evaluator: Evaluator, node: Transformation): JexlValue {
        val transform = evaluator.transforms[node.name]
            ?: throw EvaluatorException("Unknown transform ${node.name}")

        if (node.subject == null) {
            throw EvaluatorException("Missing subject for transform")
        }

        val subject = evaluator.evaluate(node.subject!!)
        val arguments = evaluator.evaluateArray(node.arguments)

        return transform.invoke(subject, arguments)
    }

    private fun evaluateFilterExpression(evaluator: Evaluator, node: FilterExpression): JexlValue {
        if (node.subject == null) {
            throw EvaluatorException("Missing subject for filter expression")
        }

        val subject = evaluator.evaluate(node.subject!!)

        if (node.expression == null) {
            throw EvaluatorException("Missing expression for filter expression")
        }

        return if (node.relative) {
            val result = evaluator.filterRelative(subject, node.expression!!)
            result
        } else {
            evaluator.filterStatic(subject, node.expression!!)
        }
    }
}
