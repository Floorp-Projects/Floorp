/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Calendar
import java.util.Calendar.MILLISECOND

@RunWith(AndroidJUnit4::class)
class StringTest {

    @Test
    fun isUrl() {
        assertTrue("mozilla.org".isUrl())
        assertTrue(" mozilla.org ".isUrl())
        assertTrue("http://mozilla.org".isUrl())
        assertTrue("https://mozilla.org".isUrl())
        assertTrue("file://somefile.txt".isUrl())
        assertTrue("http://mozilla".isUrl())
        assertTrue("http://192.168.255.255".isUrl())
        assertTrue("about:crashcontent".isUrl())
        assertTrue(" about:crashcontent ".isUrl())
        assertTrue("sample:about ".isUrl())

        assertFalse("mozilla".isUrl())
        assertFalse("mozilla android".isUrl())
        assertFalse(" mozilla android ".isUrl())
        assertFalse("Tweet:".isUrl())
        assertFalse("inurl:mozilla.org advanced search".isUrl())
        assertFalse("what is about:crashes".isUrl())

        val extraText = "Check out @asa’s Tweet: https://twitter.com/asa/status/123456789?s=09"
        val url = extraText.split(" ").find { it.isUrl() }
        assertNotEquals("Tweet:", url)
    }

    @Test
    fun toNormalizedUrl() {
        val expectedUrl = "http://mozilla.org"
        assertEquals(expectedUrl, "http://mozilla.org".toNormalizedUrl())
        assertEquals(expectedUrl, "  http://mozilla.org  ".toNormalizedUrl())
        assertEquals(expectedUrl, "mozilla.org".toNormalizedUrl())
    }

    @Test
    fun isPhone() {
        assertTrue("tel:+1234567890".isPhone())
        assertTrue(" tel:+1234567890".isPhone())
        assertTrue("tel:+1234567890 ".isPhone())
        assertTrue("tel:+1234567890 ".isPhone())
        assertTrue("TEL:+1234567890".isPhone())
        assertTrue("Tel:+1234567890".isPhone())

        assertFalse("tel:word".isPhone())
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

    @Test
    fun sha1() {
        assertEquals("da39a3ee5e6b4b0d3255bfef95601890afd80709", "".sha1())

        assertEquals("0a4d55a8d778e5022fab701977c5d840bbc486d0", "Hello World".sha1())

        assertEquals("8de545c123907e9f886ba2313560a0abef530594", "ßüöä@!§\$".sha1())
    }
}
