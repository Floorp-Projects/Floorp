/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DataUriIconLoaderTest {

    @Test
    fun `Loader returns null for http(s) urls`() {
        val loader = DataUriIconLoader()

        assertEquals(
            IconLoader.Result.NoResult,
            loader.load(
                mock(),
                mock(),
                IconRequest.Resource(
                    url = "https://www.mozilla.org",
                    type = IconRequest.Resource.Type.FAVICON,
                ),
            ),
        )

        assertEquals(
            IconLoader.Result.NoResult,
            loader.load(
                mock(),
                mock(),
                IconRequest.Resource(
                    url = "http://example.org",
                    type = IconRequest.Resource.Type.FAVICON,
                ),
            ),
        )
    }

    @Test
    fun `Loader returns bytes for data uri containing png`() {
        val loader = DataUriIconLoader()

        val result = loader.load(
            mock(),
            mock(),
            IconRequest.Resource(
                url = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91Jpz" +
                    "AAAAEklEQVR4AWP4z8AAxCDiP8N/AB3wBPxcBee7AAAAAElFTkSuQmCC",
                type = IconRequest.Resource.Type.FAVICON,
            ),
        )

        assertTrue(result is IconLoader.Result.BytesResult)

        val data = (result as IconLoader.Result.BytesResult).bytes
        assertEquals(Icon.Source.INLINE, result.source)

        assertNotNull(data)
        assertEquals(75, data.size)
    }

    @Test
    fun `Loader returns base64 decoded data`() {
        val loader = DataUriIconLoader()

        val result = loader.load(
            mock(),
            mock(),
            IconRequest.Resource(
                url = "data:image/png;base64,dGhpcyBpcyBhIHRlc3Q=",
                type = IconRequest.Resource.Type.FAVICON,
            ),
        )

        assertTrue(result is IconLoader.Result.BytesResult)

        val data = (result as IconLoader.Result.BytesResult).bytes
        assertEquals(Icon.Source.INLINE, result.source)

        val text = String(data, Charsets.UTF_8)
        assertEquals("this is a test", text)
    }
}
