/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.graphics.Bitmap
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MemoryIconLoaderTest {
    @Test
    fun `MemoryIconLoader returns bitmap from cache`() {
        val bitmap: Bitmap = mock()

        val cache = object : MemoryIconLoader.LoaderMemoryCache {
            override fun getBitmap(request: IconRequest, resource: IconRequest.Resource): Bitmap? {
                return bitmap
            }
        }

        val loader = MemoryIconLoader(cache)

        val request = IconRequest("https://www.mozilla.org")
        val resource = IconRequest.Resource(
            url = "https://www.mozilla.org/favicon.ico",
            type = IconRequest.Resource.Type.FAVICON,
        )

        val result = loader.load(mock(), request, resource)

        assertTrue(result is IconLoader.Result.BitmapResult)

        val bitmapResult = result as IconLoader.Result.BitmapResult

        assertEquals(bitmap, bitmapResult.bitmap)
    }

    @Test
    fun `MemoryIconLoader returns NoResult if cache does not contain entry`() {
        val cache = object : MemoryIconLoader.LoaderMemoryCache {
            override fun getBitmap(request: IconRequest, resource: IconRequest.Resource): Bitmap? {
                return null
            }
        }

        val loader = MemoryIconLoader(cache)

        val request = IconRequest("https://www.mozilla.org")
        val resource = IconRequest.Resource(
            url = "https://www.mozilla.org/favicon.ico",
            type = IconRequest.Resource.Type.FAVICON,
        )

        val result = loader.load(mock(), request, resource)

        assertTrue(result is IconLoader.Result.NoResult)
    }
}
