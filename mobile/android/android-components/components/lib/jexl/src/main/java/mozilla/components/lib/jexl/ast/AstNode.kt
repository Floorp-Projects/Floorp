/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.ast

/**
 * A node of the abstract syntax tree.
 *
 * This class has a lot of properties because it needs to represent all types of nodes (See [AstType]). It should be
 * possible to make this much simpler by creating a dedicated class for each type.
 */
data class AstNode(
    val type: AstType,
    var value: Any? = null,
    val operator: String? = null,
    var left: AstNode? = null,
    var right: AstNode? = null,
    var from: AstNode? = null,
    var relative: Boolean = false,
    var subject: AstNode? = null,
    var name: String? = null,
    var expression: AstNode? = null,
    var test: AstNode? = null,
    var consequent: AstNode? = null,
    var alternate: AstNode? = null,
    val arguments: MutableList<AstNode> = mutableListOf()
) {
    internal var parent: AstNode? = null

    override fun toString(): String {
        return toString(1)
    }

    @Suppress("ComplexMethod") // Yes, this method is long and complex. We should split AstNode into multiple types.
    private fun toString(level: Int): String {
        var string = ""

        val value = when {
            value is Map<*, *> -> "<Object>"
            value is List<*> -> "[Array]"
            value != null -> "< $value >"
            name != null -> "( $name )"
            else -> "[ $operator ]"
        }

        string += (value + " (" + type.toString() + ")")

        if (type == AstType.FILTER_EXPRESSION) {
            string += " [relative=$relative]"
        }

        if (this.value is Map<*, *>) {
            val obj = this.value as Map<String, AstNode>

            for ((key, value) in obj) {
                val objectValue = value.toString(level + 1)

                string += "\n" + (" ".repeat(level * 2))
                string += "$key : $objectValue"
            }
        }

        if (subject != null) {
            string += "\n" + (" ".repeat(level * 2))
            string += "subject = " + subject?.toString(level + 1)
        }

        if (test != null) {
            string += "\n" + (" ".repeat(level * 2))
            string += "test = " + test?.toString(level + 1)
        }

        if (consequent != null) {
            string += "\n" + (" ".repeat(level * 2))
            string += "consequent = " + consequent?.toString(level + 1)
        }

        if (alternate != null) {
            string += "\n" + (" ".repeat(level * 2))
            string += "alternate = " + alternate?.toString(level + 1)
        }

        if (expression != null) {
            string += "\n" + (" ".repeat(level * 2))
            string += "expr = " + expression?.toString(level + 1)
        }

        for (argument in arguments) {
            string += "\n" + (" ".repeat(level * 2))
            string += "arg = " + argument.toString(level + 1)
        }

        if (this.value is List<*>) {
            val array = this.value as List<AstNode>

            for (node in array) {
                string += "\n" + (" ".repeat((level - 1) * 2))
                string += "  " + node.toString(level + 1)
            }
        }

        if (from != null) {
            string += "\n" + (" ".repeat(level * 2))
            string += "from =  " + from?.toString(level + 1)
        }

        if (left != null) {
            string += "\n" + (" ".repeat(level * 2))
            string += "left =  " + left?.toString(level + 1)
        }

        if (right != null) {
            string += "\n" + (" ".repeat(level * 2))
            string += "right = " + right?.toString(level + 1)
        }

        return string
    }

    override fun equals(other: Any?): Boolean {
        if (other !is AstNode) {
            return false
        }

        return type == other.type &&
            value == other.value &&
            operator == other.operator &&
            left == other.left &&
            right == other.right &&
            from == other.from &&
            relative == other.relative &&
            subject == other.subject &&
            name == other.name &&
            arguments == other.arguments &&
            relative == relative
    }

    internal fun print() {
        println(toString())
    }
}
