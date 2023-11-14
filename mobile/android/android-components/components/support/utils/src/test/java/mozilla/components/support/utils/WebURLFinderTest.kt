/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.utils.WebURLFinder.Companion.isValidWebURL
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebURLFinderTest {
    private fun find(string: String?): String? {
        // Test with explicit unicode support. Implicit unicode support is available in Android
        // but not on host systems where testing will take place. See the comment in WebURLFinder
        // for additional information.
        return WebURLFinder(string, true).bestWebURL()
    }

    private fun find(strings: List<String>): String? {
        // Test with explicit unicode support. Implicit unicode support is available in Android
        // but not on host systems where testing will take place. See the comment in WebURLFinder
        // for additional information.
        return WebURLFinder(strings, true).bestWebURL()
    }

    fun testNoEmail() {
        assertNull(find("test@test.com"))
    }

    @Test
    fun testSchemeFirst() {
        assertEquals("http://scheme.com", find("test.com http://scheme.com"))
        assertEquals("http://ç.çơḿ", find("www.cnn.com http://ç.çơḿ"))
    }

    @Test
    fun testFullURL() {
        assertEquals(
            "http://scheme.com:8080/inner#anchor&arg=1",
            find("test.com http://scheme.com:8080/inner#anchor&arg=1"),
        )
        assertEquals(
            "http://s-scheme.com:8080/inner#anchor&arg=1",
            find("test.com http://s-scheme.com:8080/inner#anchor&arg=1"),
        )
        assertEquals(
            "http://t-example:8080/appversion-1.0.0/f/action.xhtml",
            find("test.com http://t-example:8080/appversion-1.0.0/f/action.xhtml"),
        )
        assertEquals(
            "http://t-example:8080/appversion-1.0.0/f/action.xhtml",
            find("http://t-example:8080/appversion-1.0.0/f/action.xhtml"),
        )
        assertEquals("http://ß.de/", find("http://ß.de/ çnn.çơḿ"))
        assertEquals("htt-p://ß.de/", find("çnn.çơḿ htt-p://ß.de/"))
    }

    @Test
    fun testNoScheme() {
        assertEquals("noscheme.com", find("noscheme.com example.com"))
        assertEquals("noscheme.com", find("-noscheme.com example.com"))
        assertEquals("n-oscheme.com", find("n-oscheme.com example.com"))
        assertEquals("n-oscheme.com", find("----------n-oscheme.com "))
        assertEquals("n-oscheme.ç", find("----------n-oscheme.ç-----------------------"))
    }

    @Test
    fun testNoBadScheme() {
        assertNull(find("file:///test javascript:///test.js"))
    }

    @Test
    fun testStrings() {
        assertEquals("http://test.com", find(listOf("http://test.com", "noscheme.com")))
        assertEquals(
            "http://test.com",
            find(listOf("noschemefirst.com", "http://test.com")),
        )
        assertEquals(
            "http://test.com/inner#test",
            find(
                listOf(
                    "noschemefirst.com",
                    "http://test.com/inner#test",
                    "http://second.org/fark",
                ),
            ),
        )
        assertEquals(
            "http://test.com",
            find(listOf("javascript:///test.js", "http://test.com")),
        )
        assertEquals("http://çnn.çơḿ", find(listOf("www.cnn.com http://çnn.çơḿ")))
    }

    @Test
    fun testIsValidWebURL() {
        assertTrue("http://test.com".isValidWebURL())
        assertTrue("hTTp://test.com".isValidWebURL())
        assertTrue("https://test.com".isValidWebURL())
        assertTrue("htTPs://test.com".isValidWebURL())
        assertTrue("about://test.com".isValidWebURL())
        assertTrue("abOUTt://test.com".isValidWebURL())
        assertTrue("data://test.com".isValidWebURL())
        assertTrue("daAtA://test.com".isValidWebURL())
        assertFalse("#http#://test.com".isValidWebURL())
        assertFalse("file:///sdcard/download".isValidWebURL())
        assertFalse("filE:///sdcard/Download".isValidWebURL())
        assertFalse("javascript:alert('Hi')".isValidWebURL())
        assertFalse("JAVascript:alert('Hi')".isValidWebURL())
        assertFalse("JAVascript:alert('Hi')".isValidWebURL())
        assertFalse("content://com.test.app/test".isValidWebURL())
        assertFalse("coNTent://com.test.app/test".isValidWebURL())
    }
}
