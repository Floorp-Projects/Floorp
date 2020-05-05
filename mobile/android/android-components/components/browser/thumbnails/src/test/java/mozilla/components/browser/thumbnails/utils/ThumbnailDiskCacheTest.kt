/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.utils

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import java.io.OutputStream

@RunWith(AndroidJUnit4::class)
class ThumbnailDiskCacheTest {

    @Test
    fun `Writing and reading bitmap bytes`() {
        val cache = ThumbnailDiskCache()
        val sessionIdOrUrl = "123"

        val bitmap: Bitmap = mock()
        Mockito.`when`(bitmap.compress(any(), ArgumentMatchers.anyInt(), any())).thenAnswer {
            Assert.assertEquals(
                Bitmap.CompressFormat.WEBP,
                it.arguments[0] as Bitmap.CompressFormat
            )
            Assert.assertEquals(90, it.arguments[1] as Int) // Quality

            val stream = it.arguments[2] as OutputStream
            stream.write("Hello World".toByteArray())
            true
        }

        cache.putThumbnailBitmap(testContext, sessionIdOrUrl, bitmap)

        val data = cache.getThumbnailData(testContext, sessionIdOrUrl)
        assertNotNull(data!!)
        Assert.assertEquals("Hello World", String(data))
    }
}
