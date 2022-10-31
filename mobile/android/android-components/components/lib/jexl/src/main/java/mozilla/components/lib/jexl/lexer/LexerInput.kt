/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.lexer

/**
 * Helper class for reading a string character by character with the ability to "peek" at upcoming characters.
 */
internal class LexerInput(private val value: String) {
    private var position: Int = 0
    private var mark: Int = 0

    /**
     * Marks the current position in the input.
     */
    fun mark() {
        mark = position
    }

    /**
     * Emits the string from the marked position to the current position.
     */
    fun emit(): String {
        return value.substring(mark, position)
    }

    /**
     * Move the current position [steps] steps ahread.
     */
    fun proceed(steps: Int = 1) {
        position += steps
    }

    /**
     * Returns true if the string starting as the current position equals [candidate].
     */
    fun peekEquals(candidate: String): Boolean {
        if (position + candidate.length > value.length) {
            return false
        }

        for (i in 0 until candidate.length) {
            if (candidate[i] != value[position + i]) {
                return false
            }
        }

        position += candidate.length

        return true
    }

    /**
     * Returns the string from the current position to [steps] ahead without moving the current position.
     */
    fun peekRange(steps: Int): String {
        if (position + steps > value.length) {
            return ""
        }

        return value.substring(position, position + steps)
    }

    /**
     * Returns the character at the current position
     */
    fun character(): Char = value[position]

    /**
     * Returns true if every character from the input has been read.
     */
    fun end() = position == value.length

    /**
     * Returns the character [steps] steps ahead.
     */
    fun peek(steps: Int): Char =
        if (position + steps == value.length) ' ' else value[position + steps]

    /**
     * Returns the previous character.
     */
    fun previous(): Char = if (position == 0) ' ' else value[position - 1]
}
