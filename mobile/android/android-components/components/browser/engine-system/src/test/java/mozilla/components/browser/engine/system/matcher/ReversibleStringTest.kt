/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.matcher

import org.junit.Assert.assertEquals
import org.junit.Test

class ReversibleStringTest {

    @Test(expected = StringIndexOutOfBoundsException::class)
    @Throws(StringIndexOutOfBoundsException::class)
    fun outOfBounds() {
        val fullStringRaw = "a"
        val fullString = ReversibleString.create(fullStringRaw)
        fullString.charAt(1)
    }

    @Test(expected = StringIndexOutOfBoundsException::class)
    @Throws(StringIndexOutOfBoundsException::class)
    fun outOfBoundsAfterSubstring() {
        val fullStringRaw = "abcd"
        val fullString = ReversibleString.create(fullStringRaw)

        val substring = fullString.substring(3)
        substring.charAt(1)
    }

    @Test(expected = StringIndexOutOfBoundsException::class)
    @Throws(StringIndexOutOfBoundsException::class)
    fun outOfBoundsSubstring() {
        val fullStringRaw = "abcd"
        val fullString = ReversibleString.create(fullStringRaw)
        fullString.substring(5)
    }

    @Test(expected = StringIndexOutOfBoundsException::class)
    @Throws(StringIndexOutOfBoundsException::class)
    fun outOfBoundsSubstringNegative() {
        val fullStringRaw = "abcd"
        val fullString = ReversibleString.create(fullStringRaw)
        fullString.substring(-1)
    }

    @Test(expected = StringIndexOutOfBoundsException::class)
    @Throws(StringIndexOutOfBoundsException::class)
    fun outOfBoundsAfterSubstringEmpty() {
        val fullStringRaw = "abcd"
        val fullString = ReversibleString.create(fullStringRaw)

        val substring = fullString.substring(4)
        substring.charAt(0)
    }

    @Test
    fun substringLength() {
        val fullStringRaw = "a"
        val fullString = ReversibleString.create(fullStringRaw)

        assertEquals("Length must match input string length", fullStringRaw.length, fullString.length())

        val sameString = fullString.substring(0)
        assertEquals("substring(0) should equal input String", fullStringRaw.length, sameString.length())
        assertEquals("substring(0) should equal input String", fullStringRaw[0], sameString.charAt(0))

        val emptyString = fullString.substring(1)
        assertEquals("Empty substring should be empty", 0, emptyString.length())
    }

    @Test
    fun forwardString() {
        val fullStringRaw = "abcd"
        val fullString = ReversibleString.create(fullStringRaw)

        assertEquals("Length must match input string length", fullStringRaw.length, fullString.length())

        for (i in 0 until fullStringRaw.length) {
            assertEquals("Character doesn't match input string character", fullStringRaw[i], fullString.charAt(i))
        }

        val substringRaw = fullStringRaw.substring(2)
        val substring = fullString.substring(2)

        for (i in 0 until substringRaw.length) {
            assertEquals("Character doesn't match input string character", substringRaw[i], substring.charAt(i))
        }
    }

    @Test
    fun reverseString() {
        val fullUnreversedStringRaw = "abcd"
        val fullStringRaw = StringBuffer(fullUnreversedStringRaw).reverse().toString()
        val fullString = ReversibleString.create(fullUnreversedStringRaw).reverse()

        assertEquals("Length must match input string length", fullStringRaw.length, fullString.length())

        for (i in 0 until fullStringRaw.length) {
            assertEquals("Character doesn't match input string character", fullStringRaw[i], fullString.charAt(i))
        }

        val substringRaw = fullStringRaw.substring(2)
        val substring = fullString.substring(2)

        for (i in 0 until substringRaw.length) {
            assertEquals("Character doesn't match input string character", substringRaw[i], substring.charAt(i))
        }
    }

    @Test
    fun reverseReversedString() {
        val fullUnreversedStringRaw = "abcd"
        val fullStringRaw = StringBuffer(fullUnreversedStringRaw).toString()
        val fullString = ReversibleString.create(fullUnreversedStringRaw).reverse().reverse()

        assertEquals("Length must match input string length", fullStringRaw.length, fullString.length())

        for (i in 0 until fullStringRaw.length) {
            assertEquals("Character doesn't match input string character", fullStringRaw[i], fullString.charAt(i))
        }
    }
}
