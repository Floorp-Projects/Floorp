/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class StringTest {

    @Test
    fun testToNormalizedUrl() {
        val expectedUrl = "http://mozilla.org"
        assertEquals(expectedUrl, "http://mozilla.org".toNormalizedUrl())
        assertEquals(expectedUrl, "  http://mozilla.org  ".toNormalizedUrl())
        assertEquals(expectedUrl, "mozilla.org".toNormalizedUrl())
    }

    @Test
    fun testIsUrl() {
        assertTrue("mozilla.org".isUrl())
        assertTrue(" mozilla.org ".isUrl())
        assertTrue("http://mozilla.org".isUrl())
        assertTrue("https://mozilla.org".isUrl())
        assertTrue("file://somefile.txt".isUrl())
        assertTrue("http://mozilla".isUrl())

        assertFalse("mozilla".isUrl())
        assertFalse("mozilla android".isUrl())
        assertFalse(" mozilla android ".isUrl())
    }
}
