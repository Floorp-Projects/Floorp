/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts.push.ext

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class StringKtTest {

    @Test
    fun `redacted endpoint contains short form in it`() {
        val endpoint =
            "https://updates.push.services.mozilla.com/wpush/v1/gAAAAABfAL4OBMRjP3O66lugUrcHT8kk4ENnJP4SE67US" +
                "kmH9NdIz__-_3PtC_V79-KwG73Y3mZye1qtnYzoJETaGQidjgbiJdXzB7u0T9BViE2b7O3oqsFJpnwvmO-CiFqKKP14vitH"

        assertTrue(endpoint.redactPartialUri().contains("redacted..."))
    }

    @Test
    fun `default schema if http is missing`() {
        assertEquals("https://www.example.com", "https://www.example.com".toNormalizedUrl())
        assertEquals("https://www.example.com", "htTPs://www.example.com".toNormalizedUrl())
        assertEquals("http://www.example.com", "HTTP://www.example.com".toNormalizedUrl())
        assertEquals("http://www.example.com", "http://www.example.com".toNormalizedUrl())
        assertEquals("http://www.example.com", "www.example.com".toNormalizedUrl())
        assertEquals("http://example.com", "example.com".toNormalizedUrl())
        assertEquals("http://example", "example".toNormalizedUrl())
        assertEquals("http://example", "  example ".toNormalizedUrl())

        assertEquals("ftp://example.com", "ftp://example.com".toNormalizedUrl())
        assertEquals("ftp://example.com", "FTP://example.com".toNormalizedUrl())

        assertEquals("http://httpexample.com", "httpexample.com".toNormalizedUrl())
        assertEquals("http://httpsexample.com", "httpsexample.com".toNormalizedUrl())
        assertEquals("http://httpsexample.com", "  httpsexample.com   ".toNormalizedUrl())
    }
}
