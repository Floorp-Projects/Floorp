/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.lexer

/**
 * A token emitted by the [Lexer].
 */
data class Token(
    val type: Type,
    val raw: String,
    val value: Any,
) {
    enum class Type {
        LITERAL,
        IDENTIFIER,
        DOT,
        OPEN_BRACKET,
        CLOSE_BRACKET,
        PIPE,
        OPEN_CURL,
        CLOSE_CURL,
        COLON,
        COMMA,
        OPEN_PAREN,
        CLOSE_PAREN,
        QUESTION,
        BINARY_OP,
        UNARY_OP,
    }
}
