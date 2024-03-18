/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.parser

import mozilla.components.lib.jexl.ast.AstNode
import mozilla.components.lib.jexl.ast.BinaryExpression
import mozilla.components.lib.jexl.ast.BranchNode
import mozilla.components.lib.jexl.ast.Identifier
import mozilla.components.lib.jexl.ast.Literal
import mozilla.components.lib.jexl.ast.OperatorNode
import mozilla.components.lib.jexl.ast.UnaryExpression
import mozilla.components.lib.jexl.grammar.Grammar
import mozilla.components.lib.jexl.lexer.Token

/**
 * JEXL parser.
 *
 * Takes a list of tokens from the lexer and transforms them into an abstract syntax tree (AST).
 */
internal class Parser(
    internal val grammar: Grammar,
    private val stopMap: Map<Token.Type, State> = mapOf(),
) {
    private var state: State = State.EXPECT_OPERAND
    internal var tree: AstNode? = null
    internal var cursor: AstNode? = null

    internal var subParser: Parser? = null
    private var parentStop: Boolean = false
    internal var currentObjectKey: String? = null

    internal var nextIdentEncapsulate: Boolean = false
    internal var nextIdentRelative: Boolean = false
    internal var relative: Boolean = false

    @Synchronized
    @Throws(ParserException::class)
    fun parse(tokens: List<Token>): AstNode? {
        parseTokens(tokens)

        return complete()
    }

    private fun complete(): AstNode? {
        if (cursor != null && !stateMachine[state]!!.completable) {
            throw ParserException("Unexpected end of expression")
        }

        if (subParser != null) {
            endSubExpression()
        }

        state = State.COMPLETE

        return if (cursor != null) {
            tree
        } else {
            null
        }
    }

    private fun parseTokens(tokens: List<Token>) {
        tokens.forEach { parseToken(it) }
    }

    @Suppress("ComplexMethod", "LongMethod", "ThrowsCount")
    private fun parseToken(token: Token): State? {
        if (state == State.COMPLETE) {
            throw ParserException("Token after parsing completed")
        }

        val stateMap = stateMachine[state]
            ?: throw ParserException("Can't continue from state: $state")

        if (stateMap.subHandler != null) {
            // Use a sub handler for this state
            if (subParser == null) {
                startSubExpression()
            }
            val stopState = subParser!!.parseToken(token)
            if (stopState != null) {
                endSubExpression()
                if (parentStop) {
                    return stopState
                }
                state = stopState
            }
        } else if (stateMap.map.containsKey(token.type)) {
            val nextState = stateMap.map.getValue(token.type)

            if (nextState.handler != null) {
                // Use handler for this transition
                nextState.handler.invoke(this, token)
            } else {
                // Use generic handler for this token type (if it exists)
                val handler = handlers[token.type]
                handler?.invoke(this, token)
            }

            nextState.state?.let { state = it }
        } else if (stopMap.containsKey(token.type)) {
            return stopMap.getValue(token.type)
        } else {
            throw ParserException("Token ${token.raw} (${token.type}) unexpected in state $state")
        }

        return null
    }

    internal fun placeAtCursor(node: AstNode) {
        if (cursor == null) {
            tree = node
        } else {
            cursor?.let { cursor ->
                if (cursor is BranchNode) {
                    cursor.right = node
                }
            }
            node.parent = cursor
        }

        cursor = node
    }

    internal fun placeBeforeCursor(node: AstNode) {
        cursor = cursor!!.parent
        placeAtCursor(node)
    }

    private fun startSubExpression() {
        var endStates = stateMachine[state]!!.endStates
        if (endStates.isEmpty()) {
            parentStop = true
            endStates = stopMap
        }
        this.subParser = Parser(grammar, endStates)
    }

    private fun endSubExpression() {
        val stateMap = stateMachine[state]!!
        val subHandler = stateMap.subHandler!!
        val subParser = this.subParser!!
        val node = subParser.complete()

        subHandler.invoke(this, node)
        this.subParser = null
    }
}

class ParserException(message: String) : Exception(message)

internal class StateMap(
    val map: Map<Token.Type, NextState> = mapOf(),
    val completable: Boolean = false,
    val subHandler: ((Parser, AstNode?) -> Unit)? = null,
    val endStates: Map<Token.Type, State> = mapOf(),
)

internal enum class State {
    EXPECT_OPERAND,
    EXPECT_BIN_OP,
    IDENTIFIER,
    SUB_EXPRESSION,
    EXPECT_OBJECT_KEY,
    TRAVERSE,
    ARRAY_VALUE,
    EXPECT_TRANSFORM,
    TERNARY_MID,
    TERNARY_END,
    COMPLETE,
    POST_TRANSFORM,
    EXPECT_KEY_VALUE_SEPARATOR,
    OBJECT_VALUE,
    ARGUMENT_VALUE,
    FILTER,
    POST_TRANSFORM_ARGUMENTS,
}

internal class NextState(
    val state: State? = null,
    val handler: ((Parser, Token) -> Unit)? = null,
)

internal val handlers: Map<Token.Type, (Parser, Token) -> Unit> = mapOf(
    Token.Type.LITERAL to { parser, token ->
        parser.placeAtCursor(
            Literal(token.value),
        )
    },

    Token.Type.BINARY_OP to { parser, token ->
        val precedence = parser.grammar.elements[token.value]?.precedence ?: 0
        var parent = parser.cursor!!.parent

        var operator = (parent as? OperatorNode)?.operator

        while (operator != null &&
            parser.grammar.elements[operator]!!.precedence > precedence
        ) {
            parser.cursor = parent
            parent = parent?.parent
            operator = (parent as? OperatorNode)?.operator
        }

        val node = BinaryExpression(
            left = parser.cursor,
            operator = token.value.toString(),
        )

        parser.cursor!!.parent = node
        parser.cursor = parent
        parser.placeAtCursor(node)
    },

    Token.Type.IDENTIFIER to { parser, token ->
        val node = Identifier(token.value)

        if (parser.nextIdentEncapsulate) {
            node.from = parser.cursor
            parser.placeBeforeCursor(node)
            parser.nextIdentEncapsulate = false
        } else {
            if (parser.nextIdentRelative) {
                node.relative = true
            }
            parser.placeAtCursor(node)
        }
    },

    Token.Type.UNARY_OP to { parser, token ->
        val node = UnaryExpression(
            operator = token.value.toString(),
        )
        parser.placeAtCursor(node)
    },

    Token.Type.DOT to { parser, _ ->
        val cursor = parser.cursor

        parser.nextIdentEncapsulate = cursor != null &&
            (cursor !is BinaryExpression || cursor.right != null) &&
            cursor !is UnaryExpression

        parser.nextIdentRelative = cursor == null || !parser.nextIdentEncapsulate

        if (parser.nextIdentRelative) {
            parser.relative = true
        }
    },
)
