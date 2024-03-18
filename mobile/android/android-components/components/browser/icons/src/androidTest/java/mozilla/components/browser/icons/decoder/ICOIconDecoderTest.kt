/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import mozilla.components.browser.icons.decoder.ICOIconDecoder
import mozilla.components.browser.icons.decoder.ico.decodeDirectoryEntries
import mozilla.components.support.images.DesiredSize
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class ICOIconDecoderTest {
    @Test
    fun testIconSizesOfMicrosoftFavicon() {
        val icon = loadIcon("microsoft_favicon.ico")
        val entries = decodeDirectoryEntries(icon, 1024)

        assertEquals(6, entries.size)

        val bitmaps = entries
            .mapNotNull { entry -> entry.toBitmap(icon) }
            .sortedBy { bitmap -> bitmap.width }

        assertEquals(6, bitmaps.size)

        assertEquals(16, bitmaps[0].width)
        assertEquals(16, bitmaps[0].height)

        assertEquals(24, bitmaps[1].width)
        assertEquals(24, bitmaps[1].height)

        assertEquals(32, bitmaps[2].width)
        assertEquals(32, bitmaps[2].height)

        assertEquals(48, bitmaps[3].width)
        assertEquals(48, bitmaps[3].height)

        assertEquals(72, bitmaps[4].width)
        assertEquals(72, bitmaps[4].height)

        assertEquals(128, bitmaps[5].width)
        assertEquals(128, bitmaps[5].height)
    }

    @Test
    fun testBestMicrosoftIconTarget192Max256() {
        val icon = loadIcon("microsoft_favicon.ico")

        val decoder = ICOIconDecoder()
        val bitmap = decoder.decode(icon, DesiredSize(192, 192, 256, 2.0f))

        assertNotNull(bitmap)

        assertEquals(128, bitmap!!.width)
        assertEquals(128, bitmap.height)
    }

    @Test
    fun testBestMicrosoftIconTarget64Max120() {
        val icon = loadIcon("microsoft_favicon.ico")

        val decoder = ICOIconDecoder()
        val bitmap = decoder.decode(icon, DesiredSize(64, 64, 120, 2.0f))

        assertNotNull(bitmap)

        assertEquals(72, bitmap!!.width)
        assertEquals(72, bitmap.height)
    }

    @Test
    fun testIconSizesOfGolemFavicon() {
        val icon = loadIcon("golem_favicon.ico")

        val entries = decodeDirectoryEntries(icon, 1024)

        assertEquals(5, entries.size)

        val bitmaps = entries
            .mapNotNull { entry -> entry.toBitmap(icon) }
            .sortedBy { bitmap -> bitmap.width }

        assertEquals(5, bitmaps.size)

        assertEquals(16, bitmaps[0].width)
        assertEquals(16, bitmaps[0].height)

        assertEquals(24, bitmaps[1].width)
        assertEquals(24, bitmaps[1].height)

        assertEquals(32, bitmaps[2].width)
        assertEquals(32, bitmaps[2].height)

        assertEquals(48, bitmaps[3].width)
        assertEquals(48, bitmaps[3].height)

        assertEquals(256, bitmaps[4].width)
        assertEquals(256, bitmaps[4].height)
    }

    @Test
    fun testIconSizesOfNvidiaFavicon() {
        val icon = loadIcon("nvidia_favicon.ico")

        val entries = decodeDirectoryEntries(icon, 1024)

        assertEquals(3, entries.size)

        val bitmaps = entries
            .mapNotNull { entry -> entry.toBitmap(icon) }
            .sortedBy { bitmap -> bitmap.width }

        assertEquals(3, bitmaps.size)

        assertEquals(16, bitmaps[0].width)
        assertEquals(16, bitmaps[0].height)

        assertEquals(32, bitmaps[1].width)
        assertEquals(32, bitmaps[1].height)

        assertEquals(48, bitmaps[2].width)
        assertEquals(48, bitmaps[2].height)
    }

    private fun loadIcon(fileName: String): ByteArray =
        javaClass.getResourceAsStream("/ico/$fileName")!!
            .buffered()
            .readBytes()
}
