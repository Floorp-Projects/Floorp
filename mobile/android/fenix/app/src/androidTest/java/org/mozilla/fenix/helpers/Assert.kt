/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.graphics.Bitmap
import android.graphics.Color
import android.util.Log
import org.junit.Assert.assertEquals
import org.mozilla.fenix.helpers.Constants.TAG

/**
 * Asserts the two bitmaps are the same by ensuring their dimensions, config, and
 * pixel data are the same (within the provided delta): this is the same metrics that
 * [Bitmap.sameAs] uses.
 */
fun assertEqualsWithDelta(expectedB: Bitmap, actualB: Bitmap, delta: Float) {
    Log.i(TAG, "assertEqualsWithDelta: Trying to verify that the Bitmap of $expectedB is equal with the Bitmap of $actualB within delta: $delta")
    assertEquals("widths should be equal", expectedB.width, actualB.width)
    assertEquals("heights should be equal", expectedB.height, actualB.height)
    assertEquals("config should be equal", expectedB.config, actualB.config)

    for (i in 0 until expectedB.width) {
        for (j in 0 until expectedB.height) {
            val ePx = expectedB.getPixel(i, j)
            val aPx = actualB.getPixel(i, j)
            val warn = "Pixel ${i}x$j"
            assertEquals("$warn a", Color.alpha(ePx).toFloat(), Color.alpha(aPx).toFloat(), delta)
            assertEquals("$warn r", Color.red(ePx).toFloat(), Color.red(aPx).toFloat(), delta)
            assertEquals("$warn g", Color.green(ePx).toFloat(), Color.green(aPx).toFloat(), delta)
            assertEquals("$warn b", Color.blue(ePx).toFloat(), Color.blue(aPx).toFloat(), delta)
        }
    }
    Log.i(TAG, "assertEqualsWithDelta: Verified that the Bitmap of $expectedB is equal with the Bitmap of $actualB within delta: $delta")
}
