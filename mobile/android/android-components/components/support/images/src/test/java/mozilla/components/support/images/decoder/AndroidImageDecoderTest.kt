/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images.decoder

import android.graphics.Bitmap
import android.util.Size
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.images.DesiredSize
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class AndroidImageDecoderTest {

    @Test
    fun `WHEN decoding PNG THEN returns non-null bitmap`() {
        val decoder = AndroidImageDecoder()

        val bitmap = decoder.decode(
            loadImage("png/mozac.png"),
            DesiredSize(
                targetSize = 32,
                minSize = 32,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNotNull(bitmap!!)
    }

    @Test
    fun `WHEN out of memory THEN returns null`() {
        val decoder = spy(AndroidImageDecoder())
        doThrow(OutOfMemoryError()).`when`(decoder).decodeBitmap(any(), anyInt())

        val bitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 64,
                minSize = 32,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNull(bitmap)
    }

    @Test
    fun `WHEN bitmap width equals zero THEN returns null`() {
        val decoder = spy(AndroidImageDecoder())
        doReturn(Size(0, 512)).`when`(decoder).decodeBitmapBounds(any())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 64,
                minSize = 32,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN bitmap height equals zero THEN returns null`() {
        val decoder = spy(AndroidImageDecoder())
        doReturn(Size(512, 0)).`when`(decoder).decodeBitmapBounds(any())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 64,
                minSize = 32,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN decoding null bitmap THEN returns null`() {
        val decoder = spy(AndroidImageDecoder())
        doReturn(null).`when`(decoder).decodeBitmap(any(), anyInt())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 64,
                minSize = 32,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN bitmap width too small THEN returns null`() {
        val size = Size(63, 250)

        val decoder = spy(AndroidImageDecoder())
        doReturn(size).`when`(decoder).decodeBitmapBounds(any())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 256,
                minSize = 64,
                maxSize = 256,
                maxScaleFactor = 1.0f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN bitmap height too small THEN returns null`() {
        val size = Size(250, 63)

        val decoder = spy(AndroidImageDecoder())
        doReturn(size).`when`(decoder).decodeBitmapBounds(any())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 256,
                minSize = 64,
                maxSize = 256,
                maxScaleFactor = 1.0f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN bitmap height size too small with maxScaleFactor THEN returns null`() {
        val size = Size(128, 64)

        val decoder = spy(AndroidImageDecoder())
        doReturn(size).`when`(decoder).decodeBitmapBounds(any())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 256,
                minSize = 64,
                maxSize = 256,
                maxScaleFactor = 0.9f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN bitmap width too large THEN returns null`() {
        val size = Size(2000, 250)

        val decoder = spy(AndroidImageDecoder())
        doReturn(size).`when`(decoder).decodeBitmapBounds(any())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 256,
                minSize = 32,
                maxSize = 256,
                maxScaleFactor = 1.0f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN bitmap height too large THEN returns null`() {
        val size = Size(250, 2000)

        val decoder = spy(AndroidImageDecoder())
        doReturn(size).`when`(decoder).decodeBitmapBounds(any())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 256,
                minSize = 32,
                maxSize = 256,
                maxScaleFactor = 1.0f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN bitmap height too large with maxScaleFactor THEN returns null`() {
        val size = Size(32, 256)

        val decoder = spy(AndroidImageDecoder())
        doReturn(size).`when`(decoder).decodeBitmapBounds(any())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 256,
                minSize = 32,
                maxSize = 256,
                maxScaleFactor = 0.9f,
            ),
        )

        assertNull(decodedBitmap)
    }

    @Test
    fun `WHEN bitmap size is good THEN returns non null`() {
        val bitmap: Bitmap = mock()
        val size = Size(128, 128)
        val decoder = spy(AndroidImageDecoder())
        doReturn(size).`when`(decoder).decodeBitmapBounds(any())
        doReturn(bitmap).`when`(decoder).decodeBitmap(any(), anyInt())

        val decodedBitmap = decoder.decode(
            ByteArray(0),
            DesiredSize(
                targetSize = 256,
                minSize = 64,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNotNull(decodedBitmap)
    }

    private fun loadImage(fileName: String): ByteArray =
        javaClass.getResourceAsStream("/$fileName")!!
            .buffered()
            .readBytes()
}
