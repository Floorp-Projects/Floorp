/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images.decoder

import mozilla.components.support.images.DesiredSize
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class OnDeviceAndroidImageDecoderTest {
    @Test
    fun decodingPNG() {
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
        assertEquals(16, bitmap.width)
        assertEquals(16, bitmap.height)
    }

    @Test
    fun decodingGIF() {
        val decoder = AndroidImageDecoder()

        val bitmap = decoder.decode(
            loadImage("gif/cat.gif"),
            DesiredSize(
                targetSize = 64,
                minSize = 64,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNotNull(bitmap!!)
        assertEquals(83 /* 250 / 3 */, bitmap.width)
        assertEquals(83 /* 250 / 3 */, bitmap.height)
    }

    @Test
    fun decodingJPEG() {
        val decoder = AndroidImageDecoder()

        val bitmap = decoder.decode(
            loadImage("jpg/tonys.jpg"),
            DesiredSize(
                targetSize = 64,
                minSize = 64,
                maxSize = 512,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNotNull(bitmap!!)
        assertEquals(67, bitmap.width)
        assertEquals(67, bitmap.height)
    }

    @Test
    fun decodingBMP() {
        val decoder = AndroidImageDecoder()

        val bitmap = decoder.decode(
            loadImage("bmp/test.bmp"),
            DesiredSize(
                targetSize = 64,
                minSize = 64,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNotNull(bitmap!!)
        assertEquals(100, bitmap.width)
        assertEquals(100, bitmap.height)
    }

    @Test
    fun decodingWEBP() {
        val decoder = AndroidImageDecoder()

        val bitmap = decoder.decode(
            loadImage("webp/test.webp"),
            DesiredSize(
                targetSize = 64,
                minSize = 64,
                maxSize = 256,
                maxScaleFactor = 2.0f,
            ),
        )

        assertNotNull(bitmap!!)
        assertEquals(64 /* 192 / 3 */, bitmap.width)
        assertEquals(64 /* 192 / 3 */, bitmap.height)
    }

    private fun loadImage(fileName: String): ByteArray =
        javaClass.getResourceAsStream("/$fileName")!!
            .buffered()
            .readBytes()
}
