/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.ast

/**
 * A node of the abstract syntax tree.
 */
sealed class AstNode {

    var parent: AstNode? = null

    open fun toString(level: Int, isTopLevel: Boolean = true): String = toString()
}

internal interface OperatorNode {
    val operator: String?
}

internal interface BranchNode {
    var right: AstNode?
}

// node types

internal data class Literal(
    val value: Any?,
) : AstNode() {

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription("< $value >", "LITERAL", level, isTopLevel)

    override fun toString() = toString(level = 0)
}

internal data class BinaryExpression(
    override val operator: String?,
    var left: AstNode?,
    override var right: AstNode? = null,
) : AstNode(), OperatorNode, BranchNode {

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription("[ $operator ]", "BINARY_EXPRESSION", level, isTopLevel) {
            appendChildNode(left, "left", level + 1)
            appendChildNode(right, "right", level + 1)
        }

    override fun toString() = toString(level = 0)
}

internal data class UnaryExpression(
    override val operator: String?,
    override var right: AstNode? = null,
) : AstNode(), OperatorNode, BranchNode {

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription("[ $operator ]", "UNARY_EXPRESSION", level, isTopLevel) {
            appendChildNode(right, "right", level + 1)
        }

    override fun toString() = toString(level = 0)
}

internal data class Identifier(
    var value: Any?,
    var from: AstNode? = null,
    var relative: Boolean = false,
) : AstNode() {

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription(
            "< $value >",
            "IDENTIFIER",
            level,
            isTopLevel,
            withinHeader = { append(" [ relative = $relative ]") },
        ) {
            appendChildNode(from, "from", level + 1)
        }

    override fun toString() = toString(level = 0)
}

internal data class ObjectLiteral(
    val properties: Map<String, AstNode>,
) : AstNode() {

    constructor(vararg properties: Pair<String, AstNode>) : this(properties.toMap())

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription("<Object>", "OBJECT_LITERAL", level, isTopLevel) {
            appendNodeMapValues(this@ObjectLiteral, level + 1)
        }

    override fun toString() = toString(level = 0)
}

internal data class ConditionalExpression(
    var test: AstNode?,
    var consequent: AstNode? = null,
    var alternate: AstNode? = null,
) : AstNode() {

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription("< ? >", "CONDITIONAL_EXPRESSION", level, isTopLevel) {
            appendChildNode(test, "test", level + 1)
            appendChildNode(consequent, "consequent", level + 1)
            appendChildNode(alternate, "alternate", level + 1)
        }

    override fun toString() = toString(level = 0)
}

internal data class ArrayLiteral(
    val values: MutableList<Any?>,
) : AstNode() {

    constructor(vararg values: Any?) : this(values.toMutableList())

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription(
            "[Array]",
            "ARRAY_LITERAL",
            level,
            isTopLevel,
            withinHeader = { append(" [ size = ${values.size} ]") },
        ) {
            appendNodeListValues(this@ArrayLiteral, level + 1)
        }

    override fun toString() = toString(level = 0)
}

internal data class Transformation(
    var name: String?,
    val arguments: MutableList<AstNode> = mutableListOf(),
    var subject: AstNode? = null,
) : AstNode() {

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription("( $name )", "TRANSFORMATION", level, isTopLevel) {
            appendChildNode(subject, "subject", level + 1)

            for (argument in arguments) {
                appendChildNode(argument, "arg", level + 1)
            }
        }

    override fun toString() = toString(level = 0)
}

internal data class FilterExpression(
    var expression: AstNode?,
    var subject: AstNode?,
    var relative: Boolean,
) : AstNode() {

    override fun toString(level: Int, isTopLevel: Boolean) =
        buildNodeDescription(
            "[ . ]",
            "FILTER_EXPRESSION",
            level,
            isTopLevel,
            withinHeader = { append(" [ relative = $relative ]") },
        ) {
            appendChildNode(expression, "expression", level + 1)
            appendChildNode(subject, "subject", level + 1)
        }

    override fun toString() = toString(level = 0)
}

// string representation helpers

@SuppressWarnings("LongParameterList")
private fun buildNodeDescription(
    value: String,
    name: String,
    level: Int,
    isTopLevel: Boolean = true,
    withinHeader: StringBuilder.() -> Unit = {},
    block: StringBuilder.() -> Unit = {},
) = buildString {
    if (isTopLevel) {
        appendLevelPad(level)
    }

    append(value)
    append(" ( $name )")

    withinHeader()
    appendLine()

    block()
}

private fun StringBuilder.appendNodeMapValues(node: ObjectLiteral, level: Int) {
    node.properties.forEach { (key, value) ->
        val objectValue = value.toString(level, isTopLevel = false)

        appendLevelPad(level)
        append("$key : $objectValue")
    }
}

private fun StringBuilder.appendNodeListValues(node: ArrayLiteral, level: Int) {
    val array = node.values

    array.withIndex().forEach { (i, child) ->
        appendLevelPad(level)

        val value = if (child is AstNode) {
            child.toString(level, isTopLevel = false)
        } else {
            child.toString()
        }

        append("$i : $value")
    }
}

private fun StringBuilder.appendChildNode(node: AstNode?, name: String, level: Int) {
    node ?: return

    appendLevelPad(level)
    append("$name = ")
    append(node.toString(level, isTopLevel = false))
}

private fun StringBuilder.appendLevelPad(level: Int) = append("".padStart(level * 2, ' '))
