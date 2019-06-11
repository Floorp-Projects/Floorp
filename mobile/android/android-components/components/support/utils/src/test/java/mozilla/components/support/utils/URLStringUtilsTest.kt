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
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class URLStringUtilsTest {

    @Test
    fun toNormalizedURL() {
        val expectedUrl = "http://mozilla.org"
        assertEquals(expectedUrl, toNormalizedURL("http://mozilla.org"))
        assertEquals(expectedUrl, toNormalizedURL("  http://mozilla.org  "))
        assertEquals(expectedUrl, toNormalizedURL("mozilla.org"))
    }

    @Test
    fun isURLLike() {
        assertTrue(isURLLike("mozilla.org"))
        assertTrue(isURLLike(" mozilla.org "))
        assertTrue(isURLLike("http://mozilla.org"))
        assertTrue(isURLLike("https://mozilla.org"))
        assertTrue(isURLLike("file://somefile.txt"))
        assertFalse(isURLLike("file://somefile.txt", true))

        assertTrue(isURLLike("http://mozilla"))
        assertTrue(isURLLike("http://192.168.255.255"))
        assertTrue(isURLLike("192.167.255.255"))
        assertTrue(isURLLike("about:crashcontent"))
        assertTrue(isURLLike(" about:crashcontent "))
        assertTrue(isURLLike("sample:about "))
        assertTrue(isURLLike("https://mozilla-mobile.com"))

        assertTrue(isURLLike("link.info"))
        assertFalse(isURLLike("link.unknown"))

        assertFalse(isURLLike("mozilla"))
        assertFalse(isURLLike("mozilla android"))
        assertFalse(isURLLike(" mozilla android "))
        assertFalse(isURLLike("Tweet:"))
        assertFalse(isURLLike("inurl:mozilla.org advanced search"))
        assertFalse(isURLLike("what is about:crashes"))

        val extraText = "Check out @asa’s Tweet: https://twitter.com/asa/status/123456789?s=09"
        val url = extraText.split(" ").find { isURLLike(it) }
        assertNotEquals("Tweet:", url)

        assertFalse(isURLLike("3.14"))
        assertFalse(isURLLike("3.14.2019"))

        assertFalse(isURLLike("file://somefile.txt", true))
        assertFalse(isURLLike("about:config", true))
    }

    @Test
    fun isSearchTerm() {
        assertTrue(isSearchTerm("inurl:mozilla.org advanced search"))
        assertTrue(isSearchTerm("3.14.2019"))

        assertFalse(isSearchTerm("about:config"))
        assertFalse(isSearchTerm("http://192.168.255.255"))
    }
}
