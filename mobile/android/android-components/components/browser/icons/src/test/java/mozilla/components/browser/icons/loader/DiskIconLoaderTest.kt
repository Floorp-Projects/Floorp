/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.content.Context
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class DiskIconLoaderTest {
    @Test
    fun `DiskIconLoader returns bitmap from cache`() {
        val cache = object : DiskIconLoader.LoaderDiskCache {
            override fun getIconData(context: Context, resource: IconRequest.Resource): ByteArray? {
                return "Hello World".toByteArray()
            }
        }

        val loader = DiskIconLoader(cache)

        val request = IconRequest("https://www.mozilla.org")
        val resource = IconRequest.Resource(
            url = "https://www.mozilla.org/favicon.ico",
            type = IconRequest.Resource.Type.FAVICON,
        )

        val result = loader.load(mock(), request, resource)

        assertTrue(result is IconLoader.Result.BytesResult)

        val bytesResult = result as IconLoader.Result.BytesResult

        assertEquals("Hello World", String(bytesResult.bytes))
    }

    @Test
    fun `DiskIconLoader returns NoResult if cache does not contain entry`() {
        val cache = object : DiskIconLoader.LoaderDiskCache {
            override fun getIconData(context: Context, resource: IconRequest.Resource): ByteArray? {
                return null
            }
        }

        val loader = DiskIconLoader(cache)

        val request = IconRequest("https://www.mozilla.org")
        val resource = IconRequest.Resource(
            url = "https://www.mozilla.org/favicon.ico",
            type = IconRequest.Resource.Type.FAVICON,
        )

        val result = loader.load(mock(), request, resource)

        assertTrue(result is IconLoader.Result.NoResult)
    }
}
