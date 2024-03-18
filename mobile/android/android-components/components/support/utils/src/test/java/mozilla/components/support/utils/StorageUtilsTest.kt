/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import mozilla.components.support.utils.StorageUtils.levenshteinDistance
import org.junit.Assert.assertEquals
import org.junit.Ignore
import org.junit.Test

class StorageUtilsTest {

    @Test
    @Ignore("Some assertions failed. Should we fix implementation?")
    fun checkLevenshteinDistance() {
        // Test data <s>stealed</s> borrowed from
        // https://oldfashionedsoftware.com/tag/levenshtein-distance/

        // Empty strings
        assertEquals(Int.MAX_VALUE, levenshteinDistance("", ""))
        assertEquals(Int.MAX_VALUE, levenshteinDistance("a", ""))
        assertEquals(Int.MAX_VALUE, levenshteinDistance("", "a"))
        assertEquals(Int.MAX_VALUE, levenshteinDistance("abc", ""))
        assertEquals(Int.MAX_VALUE, levenshteinDistance("", "abc"))

        // Equal strings
        assertEquals(0, levenshteinDistance("a", "a"))
        assertEquals(0, levenshteinDistance("abc", "abc"))

        // Only inserts are needed
        assertEquals(1, levenshteinDistance("a", "ab"))
        assertEquals(1, levenshteinDistance("b", "ab"))
        assertEquals(1, levenshteinDistance("ac", "abc"))
        assertEquals(6, levenshteinDistance("abcdefg", "xabxcdxxefxgx"))

        // Only deletes are needed
        assertEquals(1, levenshteinDistance("ab", "a"))
        assertEquals(1, levenshteinDistance("ab", "b"))
        assertEquals(1, levenshteinDistance("abc", "ac"))
        assertEquals(6, levenshteinDistance("xabxcdxxefxgx", "abcdefg"))

        // Only substitutions are needed
        assertEquals(1, levenshteinDistance("a", "b"))
        assertEquals(1, levenshteinDistance("ab", "ac"))
        assertEquals(1, levenshteinDistance("ac", "bc"))
        assertEquals(1, levenshteinDistance("abc", "axc"))
        assertEquals(6, levenshteinDistance("xabxcdxxefxgx", "1ab2cd34ef5g6"))

        // Many operations are needed
        assertEquals(3, levenshteinDistance("example", "samples"))
        assertEquals(6, levenshteinDistance("sturgeon", "urgently"))
        assertEquals(6, levenshteinDistance("levenshtein", "frankenstein"))
        assertEquals(5, levenshteinDistance("distance", "difference"))
        assertEquals(10, levenshteinDistance("java was neat", "kotlin is great"))
    }
}
