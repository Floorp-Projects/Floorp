/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.graphics.Bitmap
import android.graphics.Color
import mozilla.components.browser.icons.Icon
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn

class ColorProcessorTest {
    @Test
    fun `test extracting color`() {
        val icon = Icon(mockRedBitmap(1), source = Icon.Source.DISK)
        val processed = ColorProcessor().process(mock(), mock(), mock(), icon, mock())

        assertEquals(icon.bitmap, processed.bitmap)
        assertNotNull(processed.color)
    }

    @Test
    fun `test extracting color from larger bitmap`() {
        val icon = Icon(mockRedBitmap(3), source = Icon.Source.DISK)
        val processed = ColorProcessor().process(mock(), mock(), mock(), icon, mock())

        assertEquals(icon.bitmap, processed.bitmap)
        assertNotNull(processed.color)
    }

    private fun mockRedBitmap(size: Int): Bitmap {
        val bitmap: Bitmap = mock()
        doReturn(size).`when`(bitmap).height
        doReturn(size).`when`(bitmap).width

        doAnswer {
            val pixels: IntArray = it.getArgument(0)
            for (i in 0 until pixels.size) {
                pixels[i] = Color.RED
            }
            null
        }.`when`(bitmap).getPixels(any(), anyInt(), anyInt(), anyInt(), anyInt(), anyInt(), anyInt())

        return bitmap
    }
}
