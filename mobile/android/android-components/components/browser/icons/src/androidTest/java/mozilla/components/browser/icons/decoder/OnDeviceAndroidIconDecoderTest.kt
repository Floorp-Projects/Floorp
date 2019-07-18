/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.decoder

import mozilla.components.browser.icons.DesiredSize
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class OnDeviceAndroidIconDecoderTest {
    @Test
    fun decodingPNG() {
        val decoder = AndroidIconDecoder()

        val bitmap = decoder.decode(loadImage("png/mozac.png"), DesiredSize(
            targetSize = 32,
            maxSize = 256,
            maxScaleFactor = 2.0f
        ))

        assertNotNull(bitmap!!)
        assertEquals(16, bitmap.width)
        assertEquals(16, bitmap.height)
    }

    @Test
    fun decodingGIF() {
        val decoder = AndroidIconDecoder()

        val bitmap = decoder.decode(loadImage("gif/cat.gif"), DesiredSize(
            targetSize = 64,
            maxSize = 256,
            maxScaleFactor = 2.0f
        ))

        assertNotNull(bitmap!!)
        assertEquals(250 / 3, bitmap.width)
        assertEquals(250 / 3, bitmap.height)
    }

    @Test
    fun decodingJPEG() {
        val decoder = AndroidIconDecoder()

        val bitmap = decoder.decode(loadImage("jpg/tonys.jpg"), DesiredSize(
            targetSize = 64,
            maxSize = 512,
            maxScaleFactor = 2.0f
        ))

        assertNotNull(bitmap!!)
        assertEquals(67, bitmap.width)
        assertEquals(67, bitmap.height)
    }

    @Test
    fun decodingBMP() {
        val decoder = AndroidIconDecoder()

        val bitmap = decoder.decode(loadImage("bmp/test.bmp"), DesiredSize(
            targetSize = 64,
            maxSize = 256,
            maxScaleFactor = 2.0f
        ))

        assertNotNull(bitmap!!)
        assertEquals(100, bitmap.width)
        assertEquals(100, bitmap.height)
    }

    @Test
    fun decodingWEBP() {
        val decoder = AndroidIconDecoder()

        val bitmap = decoder.decode(loadImage("webp/test.webp"), DesiredSize(
            targetSize = 64,
            maxSize = 256,
            maxScaleFactor = 2.0f
        ))

        assertNotNull(bitmap!!)
        assertEquals(192 / 3, bitmap.width)
        assertEquals(192 / 3, bitmap.height)
    }

    private fun loadImage(fileName: String): ByteArray =
        javaClass.getResourceAsStream("/$fileName")!!
            .buffered()
            .readBytes()
}
