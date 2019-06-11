/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.progress

import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ShiftDrawableTest {

    @Test
    fun draw() {
        val canvas = mock<Canvas>()
        val wrappedDrawable = mock<Drawable>()
        val drawable = ShiftDrawable(wrappedDrawable)

        drawable.bounds = Rect(1, 2, 3, 4)
        drawable.draw(canvas)

        verify(wrappedDrawable, times(2)).draw(canvas)
        verify(canvas, times(2)).restore()
        verify(canvas).restoreToCount(any(Int::class.java))
    }
}