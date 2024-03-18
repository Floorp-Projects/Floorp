/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.matcher

/**
 * A String wrapper utility that allows for efficient string reversal. We
 * regularly need to reverse strings. The standard way of doing this in Java
 * would be to copy the string to reverse (e.g. using StringBuffer.reverse()).
 * This seems wasteful when we only read our Strings character by character,
 * in which case can just transpose positions as needed.
 */
abstract class ReversibleString private constructor(
    protected val string: String,
    protected val offsetStart: Int,
    protected val offsetEnd: Int,
) {
    abstract val isReversed: Boolean
    abstract fun charAt(position: Int): Char
    abstract fun substring(startIndex: Int): ReversibleString

    init {
        if (offsetStart > offsetEnd || offsetStart < 0 || offsetEnd < 0) {
            throw StringIndexOutOfBoundsException("Cannot create negative-length String")
        }
    }

    /**
     * Returns the length of this string.
     */
    fun length(): Int = offsetEnd - offsetStart

    /**
     * Reverses this string.
     */
    fun reverse(): ReversibleString =
        if (isReversed) {
            ForwardString(string, offsetStart, offsetEnd)
        } else {
            ReverseString(string, offsetStart, offsetEnd)
        }

    private class ForwardString(
        string: String,
        offsetStart: Int,
        offsetEnd: Int,
    ) : ReversibleString(string, offsetStart, offsetEnd) {
        override val isReversed: Boolean = false

        override fun charAt(position: Int): Char {
            if (position > length()) {
                throw StringIndexOutOfBoundsException()
            }
            return string[position + offsetStart]
        }

        override fun substring(startIndex: Int): ReversibleString {
            return ForwardString(string, offsetStart + startIndex, offsetEnd)
        }
    }

    private class ReverseString(
        string: String,
        offsetStart: Int,
        offsetEnd: Int,
    ) : ReversibleString(string, offsetStart, offsetEnd) {
        override val isReversed: Boolean = true

        override fun charAt(position: Int): Char {
            if (position > length()) {
                throw StringIndexOutOfBoundsException()
            }
            return string[length() - 1 - position + offsetStart]
        }

        override fun substring(startIndex: Int): ReversibleString {
            return ReverseString(string, offsetStart, offsetEnd - startIndex)
        }
    }

    companion object {
        /**
         * Create a [ReversibleString] for the provided [String].
         */
        fun create(string: String): ReversibleString {
            return ForwardString(string, 0, string.length)
        }
    }
}

fun String.reversible(): ReversibleString {
    return ReversibleString.create(this)
}

fun String.reverse(): ReversibleString {
    return this.reversible().reverse()
}
