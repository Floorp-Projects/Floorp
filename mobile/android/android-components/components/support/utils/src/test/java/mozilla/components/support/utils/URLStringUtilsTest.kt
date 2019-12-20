/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.utils.URLStringUtils.isSearchTerm
import mozilla.components.support.utils.URLStringUtils.isURLLike
import mozilla.components.support.utils.URLStringUtils.isURLLikeStrict
import mozilla.components.support.utils.URLStringUtils.toNormalizedURL
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
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
    fun isURLLikeStrict() {
        assertTrue(isURLLikeStrict("mozilla.org"))
        assertTrue(isURLLikeStrict(" mozilla.org "))
        assertTrue(isURLLikeStrict("http://mozilla.org"))
        assertTrue(isURLLikeStrict("https://mozilla.org"))
        assertTrue(isURLLikeStrict("file://somefile.txt"))
        assertFalse(isURLLikeStrict("file://somefile.txt", true))

        assertTrue(isURLLikeStrict("http://mozilla"))
        assertTrue(isURLLikeStrict("http://192.168.255.255"))
        assertTrue(isURLLikeStrict("192.167.255.255"))
        assertTrue(isURLLikeStrict("about:crashcontent"))
        assertTrue(isURLLikeStrict(" about:crashcontent "))
        assertTrue(isURLLikeStrict("sample:about "))
        assertTrue(isURLLikeStrict("https://mozilla-mobile.com"))

        assertTrue(isURLLikeStrict("link.info"))
        assertFalse(isURLLikeStrict("link.unknown"))

        assertFalse(isURLLikeStrict("mozilla"))
        assertFalse(isURLLikeStrict("mozilla android"))
        assertFalse(isURLLikeStrict(" mozilla android "))
        assertFalse(isURLLikeStrict("Tweet:"))
        assertFalse(isURLLikeStrict("inurl:mozilla.org advanced search"))
        assertFalse(isURLLikeStrict("what is about:crashes"))

        val extraText = "Check out @asa’s Tweet: https://twitter.com/asa/status/123456789?s=09"
        val url = extraText.split(" ").find { isURLLikeStrict(it) }
        assertNotEquals("Tweet:", url)

        assertFalse(isURLLikeStrict("3.14"))
        assertFalse(isURLLikeStrict("3.14.2019"))

        assertFalse(isURLLikeStrict("file://somefile.txt", true))
        assertFalse(isURLLikeStrict("about:config", true))
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
        assertTrue(isURLLike(" cnn.com "))
        assertTrue(isURLLike(" cnn.com"))
        assertTrue(isURLLike("cnn.com "))
        assertTrue(isURLLike("mozilla.com/~userdir"))

        assertTrue(isURLLike("http://faß.de//"))
        assertTrue(isURLLike("cnn.cơḿ"))
        assertTrue(isURLLike("cnn.çơḿ"))
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
        // Per https://bugs.chromium.org/p/chromium/issues/detail?id=31405, ICANN will accept
        // purely numeric gTLDs.
        assertFalse(isSearchTerm("3.14.2019"))
        assertFalse(isSearchTerm(" cnn.com "))
        assertFalse(isSearchTerm(" cnn.com"))
        assertFalse(isSearchTerm("cnn.com "))
        assertFalse(isSearchTerm("mozilla.com/~userdir"))
        assertFalse(isSearchTerm("http://faß.de//"))
        assertFalse(isSearchTerm("cnn.cơḿ"))
        assertFalse(isSearchTerm("cnn.çơḿ"))
    }
}
