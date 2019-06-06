/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.net

import androidx.core.net.toUri
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import androidx.test.ext.junit.runners.AndroidJUnit4

@RunWith(AndroidJUnit4::class)
class UriTest {

    @Test
    fun hostWithoutCommonPrefixes() {
        assertEquals(
            "mozilla.org",
            "https://www.mozilla.org".toUri().hostWithoutCommonPrefixes)

        assertEquals(
            "twitter.com",
            "https://mobile.twitter.com/home".toUri().hostWithoutCommonPrefixes)

        assertNull("".toUri().hostWithoutCommonPrefixes)

        assertEquals(
            "",
            "http://".toUri().hostWithoutCommonPrefixes)

        assertEquals(
            "facebook.com",
            "https://m.facebook.com/".toUri().hostWithoutCommonPrefixes)

        assertEquals(
            "github.com",
            "https://github.com/mozilla-mobile/android-components".toUri().hostWithoutCommonPrefixes)
    }

    @Test
    fun testIsHttpOrHttps() {
        // No value
        assertFalse("".toUri().isHttpOrHttps)

        // Garbage
        assertFalse("lksdjflasuf".toUri().isHttpOrHttps)

        // URLs with http/https
        assertTrue("https://www.google.com".toUri().isHttpOrHttps)
        assertTrue("http://www.facebook.com".toUri().isHttpOrHttps)
        assertTrue("https://mozilla.org/en-US/firefox/products/".toUri().isHttpOrHttps)

        // IP addresses
        assertTrue("https://192.168.0.1".toUri().isHttpOrHttps)
        assertTrue("http://63.245.215.20/en-US/firefox/products".toUri().isHttpOrHttps)

        // Other protocols
        assertFalse("ftp://people.mozilla.org".toUri().isHttpOrHttps)
        assertFalse("javascript:window.google.com".toUri().isHttpOrHttps)
        assertFalse("tel://1234567890".toUri().isHttpOrHttps)

        // No scheme
        assertFalse("google.com".toUri().isHttpOrHttps)
        assertFalse("git@github.com:mozilla/gecko-dev.git".toUri().isHttpOrHttps)
    }
}
