/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.utils

import android.content.Context
import android.graphics.Bitmap
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner
import java.io.OutputStream

@RunWith(RobolectricTestRunner::class)
class DiskCacheTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `Writing and reading resources`() {
        val cache = DiskCache()

        val resources = listOf(
            IconRequest.Resource(
                url = "https://www.mozilla.org/icon64.png",
                sizes = listOf(IconRequest.Resource.Size(64, 64)),
                mimeType = "image/png",
                type = IconRequest.Resource.Type.FAVICON
            ),
            IconRequest.Resource(
                url = "https://www.mozilla.org/icon128.png",
                sizes = listOf(IconRequest.Resource.Size(128, 128)),
                mimeType = "image/png",
                type = IconRequest.Resource.Type.FAVICON
            ),
            IconRequest.Resource(
                url = "https://www.mozilla.org/icon128.png",
                sizes = listOf(IconRequest.Resource.Size(180, 180)),
                type = IconRequest.Resource.Type.APPLE_TOUCH_ICON
            )
        )

        val request = IconRequest("https://www.mozilla.org", resources = resources)
        cache.putResources(context, request)

        val restoredResources = cache.getResources(context, request)
        assertEquals(3, restoredResources.size)
        assertEquals(resources, restoredResources)
    }

    @Test
    fun `Writing and reading bitmap bytes`() {
        val cache = DiskCache()

        val resource = IconRequest.Resource(
            url = "https://www.mozilla.org/icon64.png",
            sizes = listOf(IconRequest.Resource.Size(64, 64)),
            mimeType = "image/png",
            type = IconRequest.Resource.Type.FAVICON
        )

        val bitmap: Bitmap = mock()
        `when`(bitmap.compress(any(), anyInt(), any())).thenAnswer {
            assertEquals(Bitmap.CompressFormat.WEBP, it.arguments[0] as Bitmap.CompressFormat)
            assertEquals(90, it.arguments[1] as Int) // Quality

            val stream = it.arguments[2] as OutputStream
            stream.write("Hello World".toByteArray())
            true
        }

        cache.putIconBitmap(context, resource, bitmap)

        val data = cache.getIconData(context, resource)
        assertNotNull(data!!)
        assertEquals("Hello World", String(data))
    }
}
