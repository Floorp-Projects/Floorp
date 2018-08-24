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

    @Test
    fun testIsPhone() {
        assertTrue("tel:+1234567890".isPhone())
        assertTrue(" tel:+1234567890".isPhone())
        assertTrue("tel:+1234567890 ".isPhone())
        assertTrue("tel:+1234567890 ".isPhone())
        assertTrue("TEL:+1234567890".isPhone())
        assertTrue("Tel:+1234567890".isPhone())
    }

    @Test
    fun testIsEmail() {
        assertTrue("mailto:asa@mozilla.com".isEmail())
        assertTrue(" mailto:asa@mozilla.com".isEmail())
        assertTrue("mailto:asa@mozilla.com ".isEmail())
        assertTrue("MAILTO:asa@mozilla.com".isEmail())
        assertTrue("Mailto:asa@mozilla.com".isEmail())
    }

    @Test
    fun testGeoLocation() {
        assertTrue("geo:1,-1".isGeoLocation())
        assertTrue("geo:1,-1;u=1".isGeoLocation())
        assertTrue("geo:1,-1,0.5;u=1".isGeoLocation())
        assertTrue(" geo:1,-1".isGeoLocation())
        assertTrue("geo:1,-1 ".isGeoLocation())
        assertTrue("GEO:1,-1".isGeoLocation())
        assertTrue("Geo:1,-1".isGeoLocation())
    }
}
