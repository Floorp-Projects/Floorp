/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.graphics

import android.graphics.Bitmap
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BitmapKtTest {

    private lateinit var subject: Bitmap

    @Before
    fun setUp() {
        subject = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888)
    }

    @Ignore("convert to integration test. Robolectric's shadows are incomplete and cause this to fail.")
    @Test
    fun `WHEN withRoundedCorners is called THEN returned bitmap's corners should be transparent and center with color`() {
        val dimen = 200
        val fillColor = Color.RED

        val bitmap = Bitmap.createBitmap(dimen, dimen, Bitmap.Config.ARGB_8888).apply {
            eraseColor(fillColor)
        }
        val roundedBitmap = bitmap.withRoundedCorners(40f)

        fun assertCornersAreTransparent() {
            val cornerLocations = listOf(0, dimen - 1)

            cornerLocations.forEach { x ->
                cornerLocations.forEach { y ->
                    assertEquals(Color.TRANSPARENT, roundedBitmap.getPixel(x, y))
                }
            }
        }

        fun assertCenterIsFilled() {
            val center = dimen / 2
            assertEquals(fillColor, roundedBitmap.getPixel(center, center))
        }

        assertNotSame(bitmap, roundedBitmap)
        assertCornersAreTransparent()
        assertCenterIsFilled()
    }

    @Test
    fun `GIVEN an all red bitmap THEN pixels are all the same`() {
        subject.eraseColor(Color.RED)
        assertTrue(subject.arePixelsAllTheSame())
    }

    @Test
    fun `GIVEN an all transparent bitmap THEN pixels are all the same`() {
        subject.eraseColor(Color.TRANSPARENT)
        assertTrue(subject.arePixelsAllTheSame())
    }

    @Test
    fun `GIVEN an all red bitmap with one pixel not red THEN pixels are not all the same`() {
        subject.eraseColor(Color.RED)
        subject.setPixel(0, 1, Color.rgb(244, 0, 0))
        assertFalse(subject.arePixelsAllTheSame())
    }
}
