/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertSame
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
    fun `string to date conversion using multiple formats`() {

        assertEquals("2019-08-16T01:02".toDate("yyyy-MM-dd'T'HH:mm"), "2019-08-16T01:02".toDate())

        assertEquals("2019-08-16T01:02:03".toDate("yyyy-MM-dd'T'HH:mm"), "2019-08-16T01:02:03".toDate())

        assertEquals("2019-08-16".toDate("yyyy-MM-dd"), "2019-08-16".toDate())
    }

    @Test
    fun sha1() {
        assertEquals("da39a3ee5e6b4b0d3255bfef95601890afd80709", "".sha1())

        assertEquals("0a4d55a8d778e5022fab701977c5d840bbc486d0", "Hello World".sha1())

        assertEquals("8de545c123907e9f886ba2313560a0abef530594", "ßüöä@!§\$".sha1())
    }

    @Test
    fun `Try Get Host From Url`() {
        val urlTest = "http://www.example.com:1080/docs/resource1.html"
        val new = urlTest.tryGetHostFromUrl()
        assertEquals(new, "www.example.com")
    }

    @Test
    fun `Try Get Host From Malformed Url`() {
        val urlTest = "notarealurl"
        val new = urlTest.tryGetHostFromUrl()
        assertEquals(new, "notarealurl")
    }

    @Test
    fun isSameOriginAs() {
        // Host mismatch.
        assertFalse("https://foo.bar".isSameOriginAs("https://foo.baz"))
        // Scheme mismatch.
        assertFalse("http://127.0.0.1".isSameOriginAs("https://127.0.0.1"))
        // Port mismatch (implicit + explicit).
        assertFalse("https://foo.bar:444".isSameOriginAs("https://foo.bar"))
        // Port mismatch (explicit).
        assertFalse("https://foo.bar:444".isSameOriginAs("https://foo.bar:555"))
        // Port OK but scheme different.
        assertFalse("https://foo.bar".isSameOriginAs("http://foo.bar:443"))
        // Port OK (explicit) but scheme different.
        assertFalse("https://foo.bar:443".isSameOriginAs("ftp://foo.bar:443"))

        assertTrue("https://foo.bar".isSameOriginAs("https://foo.bar"))
        assertTrue("https://foo.bar/bobo".isSameOriginAs("https://foo.bar/obob"))
        assertTrue("https://foo.bar".isSameOriginAs("https://foo.bar:443"))
        assertTrue("https://foo.bar:333".isSameOriginAs("https://foo.bar:333"))
    }

    @Test
    fun isExtensionUrl() {
        assertTrue("moz-extension://1232-abcd".isExtensionUrl())
        assertFalse("mozilla.org".isExtensionUrl())
        assertFalse("https://mozilla.org".isExtensionUrl())
        assertFalse("http://mozilla.org".isExtensionUrl())
    }

    @Test

    fun sanitizeURL() {
        val expectedUrl = "http://mozilla.org"
        assertEquals(expectedUrl, "\nhttp://mozilla.org\n".sanitizeURL())
    }

    fun isResourceUrl() {
        assertTrue("resource://1232-abcd".isResourceUrl())
        assertFalse("mozilla.org".isResourceUrl())
        assertFalse("https://mozilla.org".isResourceUrl())
        assertFalse("http://mozilla.org".isResourceUrl())
    }

    @Test
    fun sanitizeFileName() {
        var file = "/../../../../../../../../../../directory/file.......txt"
        val fileName = "file.txt"

        assertEquals(fileName, file.sanitizeFileName())

        file = "/root/directory/file.txt"

        assertEquals(fileName, file.sanitizeFileName())

        assertEquals("file", "file".sanitizeFileName())

        assertEquals("file", "file..".sanitizeFileName())

        assertEquals("file", "file.".sanitizeFileName())

        assertEquals("file", ".file".sanitizeFileName())

        assertEquals("test.2020.12.01.txt", "test.2020.12.01.txt".sanitizeFileName())
    }

    @Test
    fun `getDataUrlImageExtension returns a default extension if one cannot be extracted from the data url`() {
        val base64Image = "data:;base64,testImage"

        val result = base64Image.getDataUrlImageExtension()

        assertEquals("jpg", result)
    }

    @Test
    fun `getDataUrlImageExtension returns an extension based on the media type included in the the data url`() {
        val base64Image = "data:image/gif;base64,testImage"

        val result = base64Image.getDataUrlImageExtension()

        assertEquals("gif", result)
    }

    @Test
    fun `ifNullOrEmpty returns the same if this CharSequence is not null and not empty`() {
        val randomString = "something"

        assertSame(randomString, randomString.ifNullOrEmpty { "something else" })
    }

    @Test
    fun `ifNullOrEmpty returns the invocation of the passed in argument if this CharSequence is null`() {
        val nullString: String? = null
        val validResult = "notNullString"

        assertSame(validResult, nullString.ifNullOrEmpty { validResult })
    }

    @Test
    fun `ifNullOrEmpty returns the invocation of the passed in argument if this CharSequence is empty`() {
        val nullString = ""
        val validResult = "notEmptyString"

        assertSame(validResult, nullString.ifNullOrEmpty { validResult })
    }
}
