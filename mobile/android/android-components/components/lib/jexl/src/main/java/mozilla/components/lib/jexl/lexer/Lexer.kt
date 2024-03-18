/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.lexer

import mozilla.components.lib.jexl.grammar.Grammar
import mozilla.components.lib.jexl.grammar.GrammarElement

internal class LexerException(message: String) : Exception(message)

/**
 * JEXL lexer for the lexical parsing of a JEXL string.
 *
 * Its responsibility is to identify the "parts of speech" of a Jexl expression, and tokenize and label each, but
 * to do only the most minimal syntax checking; the only errors the Lexer should be concerned with are if it's unable
 * to identify the utility of any of its tokens.  Errors stemming from these tokens not being in a sensible
 * configuration should be left for the Parser to handle.
 */
@Suppress("LargeClass")
internal class Lexer(private val grammar: Grammar) {
    private val negateAfter = listOf(
        Token.Type.BINARY_OP,
        Token.Type.UNARY_OP,
        Token.Type.OPEN_PAREN,
        Token.Type.OPEN_BRACKET,
        Token.Type.QUESTION,
        Token.Type.COLON,
    )

    /**
     * Splits the JEXL expression string into a list of tokens.
     */
    @Suppress("ComplexMethod", "LongMethod")
    @Throws(LexerException::class)
    fun tokenize(raw: String): List<Token> {
        val input = LexerInput(raw)
        val tokens = mutableListOf<Token>()

        var negate = false

        while (!input.end()) {
            if (negate && !input.character().isDigit() && !input.character().isWhitespace()) {
                throw LexerException("Negating non digit: " + input.character())
            }

            when {
                input.character() == '\'' -> tokens.add(readString(input, input.character()))

                input.character() == '"' -> tokens.add(readString(input, input.character()))

                input.character().isWhitespace() -> consumeWhiteSpaces(input)

                input.peekEquals("true") -> tokens.add(
                    Token(
                        Token.Type.LITERAL,
                        "true",
                        true,
                    ),
                )

                input.peekEquals("false") -> tokens.add(
                    Token(
                        Token.Type.LITERAL,
                        "false",
                        false,
                    ),
                )

                input.character() == '#' -> discardComment(input)

                input.character() == '-' -> {
                    val token = minusOrNegate(tokens)
                    if (token != null) {
                        tokens.add(token)
                    } else {
                        negate = true
                    }
                    input.proceed()
                }

                isElement(input, grammar.elements) -> tokens.add(lastFoundElementToken!!)

                input.character().isLetter() || input.character() == '_' || input.character() == '$' ->
                    tokens.add(readIdentifier(input))

                input.character().isDigit() -> {
                    tokens.add(readDigit(input, negate))
                    negate = false
                }

                else -> throw LexerException("Do not know how to proceed: " + input.character())
            }
        }

        return tokens
    }

    private var lastFoundElementToken: Token? = null

    @Suppress("ReturnCount")
    private fun isElement(input: LexerInput, elements: Map<String, GrammarElement>): Boolean {
        val max = elements.keys.map { it.length }.maxOrNull() ?: return false

        for (steps in max downTo 1) {
            val candidate = input.peekRange(steps)
            if (elements.containsKey(candidate)) {
                if (candidate.last().isLetter() && input.peek(steps).isLetter()) {
                    return false
                }

                val element = elements[candidate]!!
                lastFoundElementToken = Token(element.type, candidate, candidate)
                input.proceed(candidate.length)

                return true
            }
        }

        return false
    }

    private fun minusOrNegate(tokens: List<Token>): Token? {
        if (tokens.isEmpty()) {
            return null
        }

        if (tokens.last().type in negateAfter) {
            return null
        }

        return Token(Token.Type.BINARY_OP, "-", "-")
    }

    private fun discardComment(input: LexerInput) {
        while (!input.end() && input.character() != '\n') {
            input.proceed()
        }
    }

    private fun consumeWhiteSpaces(input: LexerInput) {
        while (!input.end() && input.character().isWhitespace()) {
            input.proceed()
        }
    }

    private fun readString(input: LexerInput, quote: Char): Token {
        input.mark()
        input.proceed()

        while (!input.end()) {
            // Very simple escaping implementation.
            if (input.character() == quote && input.previous() != '\\') {
                break
            }

            input.proceed()
        }

        input.proceed()

        val raw = input.emit()

        if (raw.last() != quote) {
            throw LexerException("String literal not closed")
        }

        val value = raw.substring(1, raw.length - 1)
            .replace("\\" + quote, quote.toString())
            .replace("\\\\", "\\")

        return Token(Token.Type.LITERAL, raw, value)
    }

    private fun readIdentifier(input: LexerInput): Token {
        input.mark()

        while (!input.end()) {
            if (!input.character().isLetterOrDigit() && input.character() != '_' && input.character() != '$') {
                break
            }

            input.proceed()
        }

        val raw = input.emit()

        return Token(Token.Type.IDENTIFIER, raw, raw)
    }

    @Suppress("ComplexMethod")
    private fun readDigit(input: LexerInput, negate: Boolean): Token {
        input.mark()

        var readDot = false

        while (!input.end()) {
            if (!input.character().isDigit() && input.character() != '.') {
                break
            } else if (input.character() == '.' && readDot) {
                break
            } else if (input.character() == '.') {
                readDot = true
            }

            input.proceed()
        }

        val raw = if (negate) {
            "-${input.emit()}"
        } else {
            input.emit()
        }

        val value: Any = if (raw.contains(".")) {
            raw.toDouble()
        } else {
            raw.toInt()
        }

        return Token(Token.Type.LITERAL, raw, value)
    }
}
