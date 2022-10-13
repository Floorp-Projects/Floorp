/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.utils

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.jakewharton.disklrucache.DiskLruCache
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.robolectric.annotation.Config
import java.io.IOException
import java.io.OutputStream

@RunWith(AndroidJUnit4::class)
class ThumbnailDiskCacheTest {

    @Test
    fun `Writing and reading bitmap bytes for sdk higher than 29`() {
        val cache = ThumbnailDiskCache()
        val request = ImageLoadRequest("123", 100)

        val bitmap: Bitmap = mock()
        `when`(bitmap.compress(any(), ArgumentMatchers.anyInt(), any())).thenAnswer {
            Assert.assertEquals(
                Bitmap.CompressFormat.WEBP_LOSSY,
                it.arguments[0] as Bitmap.CompressFormat,
            )
            Assert.assertEquals(90, it.arguments[1] as Int) // Quality

            val stream = it.arguments[2] as OutputStream
            stream.write("Hello World".toByteArray())
            true
        }

        cache.putThumbnailBitmap(testContext, request.id, bitmap)

        val data = cache.getThumbnailData(testContext, request)
        assertNotNull(data!!)
        Assert.assertEquals("Hello World", String(data))
    }

    @Config(sdk = [29])
    @Test
    fun `Writing and reading bitmap bytes for sdk lower or equal to 29`() {
        val cache = ThumbnailDiskCache()
        val request = ImageLoadRequest("123", 100)

        val bitmap: Bitmap = mock()
        `when`(bitmap.compress(any(), ArgumentMatchers.anyInt(), any())).thenAnswer {
            Assert.assertEquals(
                @Suppress("DEPRECATION") // not deprecated in sdk 29
                Bitmap.CompressFormat.WEBP,
                it.arguments[0] as Bitmap.CompressFormat,
            )
            Assert.assertEquals(90, it.arguments[1] as Int) // Quality

            val stream = it.arguments[2] as OutputStream
            stream.write("Hello World".toByteArray())
            true
        }

        cache.putThumbnailBitmap(testContext, request.id, bitmap)

        val data = cache.getThumbnailData(testContext, request)
        assertNotNull(data!!)
        Assert.assertEquals("Hello World", String(data))
    }

    @Test
    fun `Removing bitmap from disk cache`() {
        val cache = ThumbnailDiskCache()
        val request = ImageLoadRequest("123", 100)
        val bitmap: Bitmap = mock()

        cache.putThumbnailBitmap(testContext, request.id, bitmap)
        var data = cache.getThumbnailData(testContext, request)
        assertNotNull(data!!)

        cache.removeThumbnailData(testContext, request.id)
        data = cache.getThumbnailData(testContext, request)
        assertNull(data)
    }

    @Test
    fun `Clearing bitmap from disk cache`() {
        val cache = ThumbnailDiskCache()
        val request = ImageLoadRequest("123", 100)
        val bitmap: Bitmap = mock()

        cache.putThumbnailBitmap(testContext, request.id, bitmap)
        var data = cache.getThumbnailData(testContext, request)
        assertNotNull(data!!)

        cache.clear(testContext)
        data = cache.getThumbnailData(testContext, request)
        assertNull(data)
    }

    @Test
    fun `Clearing bitmap from disk catch IOException`() {
        val cache = ThumbnailDiskCache()
        val lruCache: DiskLruCache = mock()
        cache.thumbnailCache = lruCache

        `when`(lruCache.delete()).thenThrow(IOException("test"))

        cache.clear(testContext)

        assertNull(cache.thumbnailCache)
    }
}
