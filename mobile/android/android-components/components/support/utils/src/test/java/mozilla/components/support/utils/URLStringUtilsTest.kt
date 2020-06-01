/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.utils.URLStringUtils.isSearchTerm
import mozilla.components.support.utils.URLStringUtils.isURLLike
import mozilla.components.support.utils.URLStringUtils.toNormalizedURL
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class URLStringUtilsTest {

    @Before
    fun configurePatternFlags() {
        URLStringUtils.flags = URLStringUtils.UNICODE_CHARACTER_CLASS
    }

    @Test
    fun toNormalizedURL() {
        val expectedUrl = "http://mozilla.org"
        assertEquals(expectedUrl, toNormalizedURL("http://mozilla.org"))
        assertEquals(expectedUrl, toNormalizedURL("  http://mozilla.org  "))
        assertEquals(expectedUrl, toNormalizedURL("mozilla.org"))
    }

    @Test
    fun isUrlLike() {
        assertFalse(isURLLike("inurl:mozilla.org advanced search"))
        assertFalse(isURLLike("sf: help"))
        assertFalse(isURLLike("mozilla./~"))
        assertFalse(isURLLike("cnn.com politics"))

        assertTrue(isURLLike("about:config"))
        assertTrue(isURLLike("about:config:8000"))
        assertTrue(isURLLike("file:///home/user/myfile.html"))
        assertTrue(isURLLike("file://////////////home//user/myfile.html"))
        assertTrue(isURLLike("file://C:\\Users\\user\\myfile.html"))
        assertTrue(isURLLike("http://192.168.255.255"))
        assertTrue(isURLLike("link.unknown"))
        // Per https://bugs.chromium.org/p/chromium/issues/detail?id=31405, ICANN will accept
        // purely numeric gTLDs.
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

        // Examples from the code comments:
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
    }

    @Test
    fun isSearchTerm() {
        assertTrue(isSearchTerm("inurl:mozilla.org advanced search"))
        assertTrue(isSearchTerm("sf: help"))
        assertTrue(isSearchTerm("mozilla./~"))
        assertTrue(isSearchTerm("cnn.com politics"))

        assertFalse(isSearchTerm("about:config"))
        assertFalse(isSearchTerm("about:config:8000"))
        assertFalse(isSearchTerm("file:///home/user/myfile.html"))
        assertFalse(isSearchTerm("file://////////////home//user/myfile.html"))
        assertFalse(isSearchTerm("file://C:\\Users\\user\\myfile.html"))
        assertFalse(isSearchTerm("http://192.168.255.255"))
        assertFalse(isSearchTerm("link.unknown"))
        // Per https://bugs.chromium.org/p/chromium/issues/detail?id=31405, ICANN will accept
        // purely numeric gTLDs.
        assertFalse(isSearchTerm("3.14.2019"))
        assertFalse(isSearchTerm("3-four.14.2019"))
        assertFalse(isSearchTerm(" cnn.com "))
        assertFalse(isSearchTerm(" cnn.com"))
        assertFalse(isSearchTerm("cnn.com "))
        assertFalse(isSearchTerm("my-domain.com"))
        assertFalse(isSearchTerm("camp-firefox.de"))
        assertFalse(isSearchTerm("http://my-domain.com"))
        assertFalse(isSearchTerm("mozilla.com/~userdir"))
        assertFalse(isSearchTerm("http://faß.de//"))
        assertFalse(isSearchTerm("cnn.cơḿ"))
        assertFalse(isSearchTerm("cnn.çơḿ"))

        // Examples from the code comments:
        assertFalse(isSearchTerm("c-c.com"))
        assertFalse(isSearchTerm("c-c-c-c.c-c-c"))
        assertFalse(isSearchTerm("c-http://c.com"))
        assertFalse(isSearchTerm("about-mozilla:mozilla"))
        assertFalse(isSearchTerm("c-http.d-x"))
        assertFalse(isSearchTerm("www.c.-"))
        assertFalse(isSearchTerm("3-3.3"))
        assertFalse(isSearchTerm("www.c-c.-"))

        assertTrue(isSearchTerm(" -://x.com "))
        assertTrue(isSearchTerm("  -x.com"))
        assertTrue(isSearchTerm("http://www-.com"))
        assertTrue(isSearchTerm("www.c-c-  "))
        assertTrue(isSearchTerm("3-3 "))
    }

    @Test
    fun stripUrlSchemeUrlWithHttps() {
        val testDisplayUrl = URLStringUtils.toDisplayUrl("https://mozilla.com")
        assertEquals("mozilla.com", testDisplayUrl)
    }

    @Test
    fun stripUrlSchemeUrlWithHttp() {
        val testDisplayUrl = URLStringUtils.toDisplayUrl("http://mozilla.com")
        assertEquals("mozilla.com", testDisplayUrl)
    }

    @Test
    fun stripUrlSubdomainUrlWithHttps() {
        val testDisplayUrl = URLStringUtils.toDisplayUrl("https://www.mozilla.com")
        assertEquals("mozilla.com", testDisplayUrl)
    }

    @Test
    fun stripUrlSubdomainUrlWithHttp() {
        val testDisplayUrl = URLStringUtils.toDisplayUrl("http://www.mozilla.com")
        assertEquals("mozilla.com", testDisplayUrl)
    }

    @Test
    fun stripUrlSchemeAndSubdomainUrlNoMatch() {
        val testDisplayUrl = URLStringUtils.toDisplayUrl("zzz://www.mozillahttp://.com")
        assertEquals("zzz://www.mozillahttp://.com", testDisplayUrl)
    }
}
