/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Rect
import android.graphics.drawable.Drawable
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DrawableTest {
    @Test
    fun testSetBounds() {
        val drawable = TestDrawable()
        assertFalse(drawable.boundsChanged)

        val size = 10
        drawable.setBounds(size)
        assertTrue(drawable.boundsChanged)

        val returnRec = drawable.copyBounds()
        assertTrue(returnRec.contains(0, 0, -10, 10))
    }

    private class TestDrawable : Drawable() {
        var boundsChanged: Boolean = false
        override fun getOpacity(): Int {
            return 0
        }

        override fun draw(canvas: Canvas) {}
        override fun setAlpha(alpha: Int) {}
        override fun setColorFilter(cf: ColorFilter?) {}
        override fun onBoundsChange(bounds: Rect) {
            boundsChanged = true
            super.onBoundsChange(bounds)
        }
    }
}
