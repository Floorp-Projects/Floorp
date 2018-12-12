/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.Calendar
import java.util.Calendar.MILLISECOND

@RunWith(RobolectricTestRunner::class)
class StringTest {

    @Test
    fun toNormalizedUrl() {
        val expectedUrl = "http://mozilla.org"
        assertEquals(expectedUrl, "http://mozilla.org".toNormalizedUrl())
        assertEquals(expectedUrl, "  http://mozilla.org  ".toNormalizedUrl())
        assertEquals(expectedUrl, "mozilla.org".toNormalizedUrl())
    }

    @Test
    fun isUrl() {
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
    fun isPhone() {
        assertTrue("tel:+1234567890".isPhone())
        assertTrue(" tel:+1234567890".isPhone())
        assertTrue("tel:+1234567890 ".isPhone())
        assertTrue("tel:+1234567890 ".isPhone())
        assertTrue("TEL:+1234567890".isPhone())
        assertTrue("Tel:+1234567890".isPhone())
    }

    @Test
    fun isEmail() {
        assertTrue("mailto:asa@mozilla.com".isEmail())
        assertTrue(" mailto:asa@mozilla.com".isEmail())
        assertTrue("mailto:asa@mozilla.com ".isEmail())
        assertTrue("MAILTO:asa@mozilla.com".isEmail())
        assertTrue("Mailto:asa@mozilla.com".isEmail())
    }

    @Test
    fun geoLocation() {
        assertTrue("geo:1,-1".isGeoLocation())
        assertTrue("geo:1,-1;u=1".isGeoLocation())
        assertTrue("geo:1,-1,0.5;u=1".isGeoLocation())
        assertTrue(" geo:1,-1".isGeoLocation())
        assertTrue("geo:1,-1 ".isGeoLocation())
        assertTrue("GEO:1,-1".isGeoLocation())
        assertTrue("Geo:1,-1".isGeoLocation())
    }

    @Test
    fun toDate() {
        val calendar = Calendar.getInstance()
        calendar.set(2019, 10, 29, 0, 0, 0)
        calendar[MILLISECOND] = 0
        assertEquals(calendar.time, "2019-11-29".toDate("yyyy-MM-dd"))
        calendar.set(2019, 10, 28, 0, 0, 0)
        assertEquals(calendar.time, "2019-11-28".toDate("yyyy-MM-dd"))
        assertNotNull("".toDate("yyyy-MM-dd"))
    }
}
