/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.graphics.Bitmap
import android.util.Size
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BitmapTest {

    @Test
    fun `WHEN a square bitmap is smaller than the given max size THEN resizeMaintainingAspectRatio scales the size up maintaining the aspect ratio`() {
        val maxSize = Size(48, 48)
        val bitmapSize = Size(24, 24)
        val bitmap = createBitmap(bitmapSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        assertEquals(maxSize, scaledBitmapSize)
    }

    @Test
    fun `WHEN a square bitmap is same as the given max size THEN resizeMaintainingAspectRatio returns the original bitmap size`() {
        val maxSize = Size(48, 48)
        val bitmap = createBitmap(maxSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        assertEquals(maxSize, scaledBitmapSize)
    }

    @Test
    fun `WHEN a square bitmap is larger than the given max size THEN resizeMaintainingAspectRatio scales the size down maintaining the aspect ratio`() {
        val maxSize = Size(48, 48)
        val bitmapSize = Size(96, 96)
        val bitmap = createBitmap(bitmapSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        assertEquals(maxSize, scaledBitmapSize)
    }

    @Test
    fun `WHEN a wide bitmap is smaller than the given max size THEN resizeMaintainingAspectRatio scales the size up maintaining the aspect ratio`() {
        val maxSize = Size(48, 48)
        val bitmapSize = Size(24, 12)
        val bitmap = createBitmap(bitmapSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        val expected = Size(48, 24)
        assertEquals(expected, scaledBitmapSize)
    }

    @Test
    fun `WHEN a wide bitmap is same as the given max size THEN resizeMaintainingAspectRatio returns the bitmap original size`() {
        val maxSize = Size(48, 48)
        val bitmapSize = Size(48, 24)
        val bitmap = createBitmap(bitmapSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        assertEquals(bitmapSize, scaledBitmapSize)
    }

    @Test
    fun `WHEN a wide bitmap is larger than the given max size THEN resizeMaintainingAspectRatio scales the size down maintaining the aspect ratio`() {
        val maxSize = Size(48, 48)
        val bitmapSize = Size(192, 96)
        val bitmap = createBitmap(bitmapSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        val expected = Size(48, 24)
        assertEquals(expected, scaledBitmapSize)
    }

    @Test
    fun `WHEN a tall bitmap is smaller than the given max size THEN resizeMaintainingAspectRatio scales the size up maintaining the aspect ratio`() {
        val maxSize = Size(48, 48)
        val bitmapSize = Size(12, 24)
        val bitmap = createBitmap(bitmapSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        val expected = Size(24, 48)
        assertEquals(expected, scaledBitmapSize)
    }

    @Test
    fun `WHEN a tall bitmap is same as the given max size THEN resizeMaintainingAspectRatio returns the bitmap original size`() {
        val maxSize = Size(48, 48)
        val bitmapSize = Size(24, 48)
        val bitmap = createBitmap(bitmapSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        assertEquals(bitmapSize, scaledBitmapSize)
    }

    @Test
    fun `WHEN a tall bitmap is larger than the given max size THEN resizeMaintainingAspectRatio scales the size down maintaining the aspect ratio`() {
        val maxSize = Size(48, 48)
        val bitmapSize = Size(96, 192)
        val bitmap = createBitmap(bitmapSize)

        val scaledBitmapSize = bitmap.resizeMaintainingAspectRatio(maxSize)

        val expected = Size(24, 48)
        assertEquals(expected, scaledBitmapSize)
    }

    private fun createBitmap(size: Size) = with(size) {
        Bitmap.createBitmap(IntArray(width * height), width, height, Bitmap.Config.ARGB_8888)
    }
}
