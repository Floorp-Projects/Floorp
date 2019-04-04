/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class DataUriIconLoaderTest {
    @Test
    fun `Loader returns null for http(s) urls`() {
        val loader = DataUriIconLoader()

        assertNull(loader.load(mock(), IconRequest.Resource(
            url = "https://www.mozilla.org",
            type = IconRequest.Resource.Type.FAVICON
        )))

        assertNull(loader.load(mock(), IconRequest.Resource(
            url = "http://example.org",
            type = IconRequest.Resource.Type.FAVICON
        )))
    }

    @Test
    fun `Loader returns bytes for data uri containing png`() {
        val loader = DataUriIconLoader()

        val data = loader.load(mock(), IconRequest.Resource(
            url = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91Jpz" +
                "AAAAEklEQVR4AWP4z8AAxCDiP8N/AB3wBPxcBee7AAAAAElFTkSuQmCC",
            type = IconRequest.Resource.Type.FAVICON
        ))

        assertNotNull(data!!)
        assertEquals(75, data.size)
    }

    @Test
    fun `Loader returns base64 decoded data`() {
        val loader = DataUriIconLoader()

        val data = loader.load(mock(), IconRequest.Resource(
            url = "data:image/png;base64,dGhpcyBpcyBhIHRlc3Q=",
            type = IconRequest.Resource.Type.FAVICON
        ))

        assertNotNull(data!!)

        val text = String(data, Charsets.UTF_8)
        assertEquals("this is a test", text)
    }

    @Test
    fun `Loader has source INLINE`() {
        assertEquals(Icon.Source.INLINE, DataUriIconLoader().source)
    }
}
