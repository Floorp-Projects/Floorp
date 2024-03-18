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
        assertEquals(
            "http://[2001:db8::1.2.3.4]:8080/inner#anchor&arg=1",
            find("test.com http://[2001:db8::1.2.3.4]:8080/inner#anchor&arg=1"),
        )
        assertEquals("http://[::]", find("test.com http://[::]"))
    }

    @Test
    fun testNoScheme() {
        assertEquals("noscheme.com", find("noscheme.com example.com"))
        assertEquals("noscheme.com", find("-noscheme.com example.com"))
        assertEquals("n-oscheme.com", find("n-oscheme.com example.com"))
        assertEquals("n-oscheme.com", find("----------n-oscheme.com "))
        assertEquals("n-oscheme.ç", find("----------n-oscheme.ç-----------------------"))

        // We would ideally test "[::] example.com" here, but java.net.URI
        // doesn't seem to accept IPv6 literals without a scheme.
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

    @Test
    fun isUrlLikeEmulated() {
        // autolinkWebUrlPattern uses a copy of the regex from URLStringUtils,
        // so here we emulate isURLLike() and copy its tests.
        val isURLLike: (String) -> Boolean = {
            find("random_text $it other_random_text") == it.trim()
        }

        assertFalse(isURLLike("inurl:mozilla.org advanced search"))
        assertFalse(isURLLike("sf: help"))
        assertFalse(isURLLike("mozilla./~"))
        assertFalse(isURLLike("cnn.com politics"))

        assertTrue(isURLLike("about:config"))
        assertTrue(isURLLike("about:config:8000"))

        // These cases differ from the original isUrlLike test because
        // file:// is rejected by isInvalidUriScheme.
        assertFalse(isURLLike("file:///home/user/myfile.html"))
        assertFalse(isURLLike("file://////////////home//user/myfile.html"))
        assertFalse(isURLLike("file://C:\\Users\\user\\myfile.html"))

        assertTrue(isURLLike("http://192.168.255.255"))
        assertTrue(isURLLike("link.unknown"))
        assertTrue(isURLLike("3.14.2019"))
        assertTrue(isURLLike("3-four.14.2019"))
        assertTrue(isURLLike(" cnn.com "))
        assertTrue(isURLLike(" cnn.com"))
        assertTrue(isURLLike("cnn.com "))
        assertTrue(isURLLike("mozilla.com/~userdir"))
        assertTrue(isURLLike("my-domain.com"))
        assertTrue(isURLLike("http://faß.de//"))
        assertTrue(isURLLike("cnn.cơḿ"))
        assertTrue(isURLLike("cnn.çơḿ"))

        assertTrue(isURLLike("c-c.com"))
        assertTrue(isURLLike("c-c-c-c.c-c-c"))
        assertTrue(isURLLike("c-http://c.com"))
        assertTrue(isURLLike("about-mozilla:mozilla"))
        assertTrue(isURLLike("c-http.d-x"))
        assertTrue(isURLLike("www.c.-"))
        assertTrue(isURLLike("3-3.3"))
        assertTrue(isURLLike("www.c-c.-"))

        assertFalse(isURLLike(" -://x.com "))
        assertFalse(isURLLike("  -x.com"))
        assertFalse(isURLLike("http://www-.com"))
        assertFalse(isURLLike("www.c-c-  "))
        assertFalse(isURLLike("3-3 "))

        val validIPv6Literals = listOf(
            "[::]",
            "[::1]",
            "[1::]",
            "[1:2:3:4:5:6:7:8]",
            "[2001:db8::1.2.3.4]",
            "[::1]:8080",
        )

        validIPv6Literals.forEach { url ->
            // These cases differ from the original isUrlLike test because
            // java.net.URI doesn't recognize bare IPv6 literals.
            assertFalse(isURLLike(url))
            assertFalse(isURLLike("$url/"))

            assertTrue(isURLLike("https://$url"))
            assertTrue(isURLLike("https://$url/"))
            assertTrue(isURLLike("https:$url"))
            assertTrue(isURLLike("https:$url/"))
            assertTrue(isURLLike("http://$url"))
            assertTrue(isURLLike("http://$url/"))
            assertTrue(isURLLike("http:$url"))
            assertTrue(isURLLike("http:$url/"))
        }

        assertFalse(isURLLike("::1"))
        assertFalse(isURLLike(":::"))
        assertFalse(isURLLike("[[http://]]"))
        assertFalse(isURLLike("[[["))
        assertFalse(isURLLike("[[[:"))
        assertFalse(isURLLike("[[[:/"))
        assertFalse(isURLLike("http://]]]"))

        assertTrue(isURLLike("fe80::"))
        assertTrue(isURLLike("x:["))

        // These cases differ from the original isUrlLike test because
        // the regex is just an approximation. When bug 1685152 is fixed,
        // the original isURLLike will also return false.
        assertFalse(isURLLike("[:::"))
        assertFalse(isURLLike("http://[::"))
        assertFalse(isURLLike("http://[::/path"))
        assertFalse(isURLLike("http://[::?query"))
        assertFalse(isURLLike("[[http://banana]]"))
        assertFalse(isURLLike("http://[[["))
        assertFalse(isURLLike("[[[::"))
        assertFalse(isURLLike("[[[::/"))
        assertFalse(isURLLike("http://[1.2.3]"))
        assertFalse(isURLLike("https://[1:2:3:4:5:6:7]/"))
        assertFalse(isURLLike("https://[1:2:3:4:5:6:7:8:9]/"))

        assertTrue(isURLLike("https://abc--cba.com/"))
    }
}
