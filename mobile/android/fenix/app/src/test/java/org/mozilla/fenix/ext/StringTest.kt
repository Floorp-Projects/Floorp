/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import org.junit.Assert.assertEquals
import org.junit.Test

class StringTest {

    @Test
    fun `Simplified Url`() {
        val urlTest = "https://www.amazon.com"
        val new = urlTest.simplifiedUrl()
        assertEquals(new, "amazon.com")
    }

    @Test
    fun testReplaceConsecutiveZeros() {
        assertEquals(
            "2001:db8::ff00:42:8329",
            "2001:db8:0:0:0:ff00:42:8329".replaceConsecutiveZeros(),
        )
    }
}
