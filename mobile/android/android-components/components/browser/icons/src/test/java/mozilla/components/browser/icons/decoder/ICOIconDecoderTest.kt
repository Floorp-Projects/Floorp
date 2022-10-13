/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.decoder

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.icons.decoder.ico.IconDirectoryEntry
import mozilla.components.browser.icons.decoder.ico.decodeDirectoryEntries
import mozilla.components.support.images.DesiredSize
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ICOIconDecoderTest {

    @Test
    fun `Decoding microsoft favicon`() {
        val icon = loadIcon("microsoft_favicon.ico")
        val decoder = ICOIconDecoder()

        val bitmap = decoder.decode(icon, DesiredSize(192, 192, 1024, 2.0f))
        assertNotNull(bitmap)
    }

    @Test
    fun `Decoding golem favicon`() {
        val icon = loadIcon("golem_favicon.ico")
        val decoder = ICOIconDecoder()

        val bitmap = decoder.decode(icon, DesiredSize(192, 192, 1024, 2.0f))
        assertNotNull(bitmap)
    }

    @Test
    fun `Decoding nivida favicon`() {
        val icon = loadIcon("nvidia_favicon.ico")
        val decoder = ICOIconDecoder()

        val bitmap = decoder.decode(icon, DesiredSize(96, 96, 1024, 2.0f))
        assertNotNull(bitmap)
    }

    @Test
    fun testGolemIconEntries() {
        val icon = loadIcon("golem_favicon.ico")

        val entries = decodeDirectoryEntries(icon, 1024)
        assertEquals(5, entries.size)

        val expectedEntries = arrayOf(
            IconDirectoryEntry(
                16,
                16,
                0,
                32,
                1128,
                39520,
                false,
                4,
            ),
            IconDirectoryEntry(
                24,
                24,
                0,
                32,
                2488,
                37032,
                false,
                3,
            ),
            IconDirectoryEntry(
                32,
                32,
                0,
                32,
                4392,
                32640,
                false,
                2,
            ),
            IconDirectoryEntry(
                48,
                48,
                0,
                32,
                9832,
                22808,
                false,
                1,
            ),
            IconDirectoryEntry(
                256,
                256,
                0,
                32,
                22722,
                86,
                true,
                0,
            ),
        )

        entries
            .sortedBy { entry -> entry.width }
            .forEachIndexed { index, entry ->
                assertEquals(expectedEntries[index], entry)
            }
    }

    @Test
    fun testNvidiaIconEntries() {
        val icon = loadIcon("nvidia_favicon.ico")

        val entries = decodeDirectoryEntries(icon, 1024)
        assertEquals(3, entries.size)

        // Verify the best entry is correctly chosen for each width. We expect 32 bpp in all cases.
        val expectedEntries = arrayOf(
            IconDirectoryEntry(
                16,
                16,
                0,
                32,
                1128,
                24086,
                false,
                8,
            ),
            IconDirectoryEntry(
                32,
                32,
                0,
                32,
                4264,
                19822,
                false,
                7,
            ),
            IconDirectoryEntry(
                48,
                48,
                0,
                32,
                9640,
                10182,
                false,
                6,
            ),
        )

        entries
            .sortedBy { entry -> entry.width }
            .forEachIndexed { index, entry -> assertEquals(expectedEntries[index], entry) }
    }

    @Test
    fun testMicrosoftIconEntries() {
        val icon = loadIcon("microsoft_favicon.ico")

        val entries = decodeDirectoryEntries(icon, 1024)
        assertEquals(6, entries.size)

        val expectedEntries = arrayOf(
            IconDirectoryEntry(
                128,
                128,
                16,
                0,
                10344,
                102,
                false,
                0,
            ),
            IconDirectoryEntry(
                72,
                72,
                16,
                0,
                3560,
                10446,
                false,
                1,
            ),
            IconDirectoryEntry(
                48,
                48,
                16,
                0,
                1640,
                14006,
                false,
                2,
            ),
            IconDirectoryEntry(
                32,
                32,
                16,
                0,
                744,
                15646,
                false,
                3,
            ),
            IconDirectoryEntry(
                24,
                24,
                16,
                0,
                488,
                16390,
                false,
                4,
            ),
            IconDirectoryEntry(
                16,
                16,
                16,
                0,
                296,
                16878,
                false,
                5,
            ),
        )

        entries
            .sortedByDescending { entry -> entry.width }
            .forEachIndexed { index, entry -> assertEquals(expectedEntries[index], entry) }
    }

    private fun loadIcon(fileName: String): ByteArray =
        javaClass.getResourceAsStream("/ico/$fileName")!!
            .buffered()
            .readBytes()
}
