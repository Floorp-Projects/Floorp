/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.evaluator

import mozilla.components.lib.jexl.ast.AstNode
import mozilla.components.lib.jexl.ast.AstType
import mozilla.components.lib.jexl.value.JexlArray
import mozilla.components.lib.jexl.value.JexlBoolean
import mozilla.components.lib.jexl.value.JexlDouble
import mozilla.components.lib.jexl.value.JexlInteger
import mozilla.components.lib.jexl.value.JexlObject
import mozilla.components.lib.jexl.value.JexlString
import mozilla.components.lib.jexl.value.JexlUndefined
import mozilla.components.lib.jexl.value.JexlValue

internal typealias EvaluatorFunction = (Evaluator, AstNode) -> JexlValue

/**
 * A mapping of AST node type to a function that can evaluate this type of node.
 *
 * This mapping could be moved inside [Evaluator].
 */
internal object EvaluatorHandlers {
    private val functions: Map<AstType, EvaluatorFunction> = mapOf(
        AstType.LITERAL to { _, node ->
            val value = node.value
            when (value) {
                is String -> JexlString(value)
                is Double -> JexlDouble(value)
                is Int -> JexlInteger(value)
                is Boolean -> JexlBoolean(value)
                else -> throw EvaluatorException("Unknown value type: ${value!!::class}")
            }
        },

        AstType.BINARY_EXPRESSION to { evaluator, node ->
            val left = evaluator.evaluate(node.left!!)
            val right = evaluator.evaluate(node.right!!)

            val operator = evaluator.grammar.elements[node.operator!!]!!

            operator.evaluate?.invoke(left, right)
                ?: throw EvaluatorException("Can't evaluate operator: ${node.operator}")
        },

        AstType.IDENTIFIER to { evaluator, node ->
            if (node.from != null) {
                val subContext = evaluator.evaluate(node.from!!)

                when (subContext) {
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
            } else {
                if (node.relative) {
                    evaluator.relativeContext.value[(node.value.toString())]
                        ?: JexlUndefined()
                } else {
                    evaluator.context.get(node.value.toString())
                }
            }
        },

        AstType.TRANSFORM to { evaluator, node ->
            val transform = evaluator.transforms[node.name]
                ?: throw EvaluatorException("Unknown transform ${node.name}")

            if (node.subject == null) {
                throw EvaluatorException("Missing subject for transform")
            }

            val subject = evaluator.evaluate(node.subject!!)
            val arguments = evaluator.evaluateArray(node.arguments)

            transform.invoke(subject, arguments)
        },

        AstType.FILTER_EXPRESSION to { evaluator, node ->
            if (node.subject == null) {
                throw EvaluatorException("Missing subject for filter expression")
            }

            val subject = evaluator.evaluate(node.subject!!)

            if (node.expression == null) {
                throw EvaluatorException("Missing expression for filter expression")
            }

            if (node.relative) {
                val result = evaluator.filterRelative(subject, node.expression!!)
                result
            } else {
                evaluator.filterStatic(subject, node.expression!!)
            }
        },

        AstType.OBJECT_LITERAL to { evaluator, node ->
            @Suppress("UNCHECKED_CAST")
            val properties = evaluator.evaluateObject(node.value as Map<String, AstNode>)
            JexlObject(properties)
        },

        AstType.ARRAY_LITERAL to { evaluator, node ->
            @Suppress("UNCHECKED_CAST")
            val values = evaluator.evaluateArray(node.value as List<AstNode>)
            JexlArray(values)
        },

        AstType.CONDITIONAL_EXPRESSION to { evaluator, node ->
            val result = evaluator.evaluate(node.test!!)

            if (result.toBoolean()) {
                if (node.consequent != null) {
                    evaluator.evaluate(node.consequent!!)
                } else {
                    result
                }
            } else {
                evaluator.evaluate(node.alternate!!)
            }
        }
    )

    fun get(type: AstType): EvaluatorFunction {
        return functions[type]
            ?: throw EvaluatorException("Can't evaluate type $type")
    }
}
