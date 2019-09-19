/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.net.Uri
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class UriKtTest {

    @Test
    fun `extracts scheme, host and port`() {
        assertEquals("https://example.com", "https://example.com".toUri().toOrigin())
        assertEquals("http://mozilla.org:80", "http://mozilla.org:80".toUri().toOrigin())
        assertEquals("http://localhost:8080", "http://localhost:8080".toUri().toOrigin())
    }

    @Test
    fun `removes user info`() {
        assertEquals("https://example.com", "https://bob@example.com".toUri().toOrigin())
        assertEquals("http://google.com", "HTTP://bob:pass@google.com".toUri().toOrigin())
    }

    @Test
    fun `removes path`() {
        assertEquals("https://example.com", "https://example.com/".toUri().toOrigin())
        assertEquals("http://google.com", "http://google.com/search".toUri().toOrigin())
        assertEquals("http://firefox.com", "http://firefox.com/en-US/foo".toUri().toOrigin())
    }

    @Test
    fun `preserves missing scheme`() {
        assertNull("example.com".toUri().toOrigin())
        assertNull("/foo/bar".toUri().toOrigin())
    }

    private fun assertEquals(expected: String, actual: Uri?) = assertEquals(expected.toUri(), actual)
}
