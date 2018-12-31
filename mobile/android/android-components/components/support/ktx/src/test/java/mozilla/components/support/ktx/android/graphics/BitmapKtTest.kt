/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.graphics

import android.graphics.Bitmap
import android.graphics.Color
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BitmapKtTest {

    @Ignore // TODO: convert to integration test. Robolectric's shadows are incomplete and cause this to fail.
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
}
