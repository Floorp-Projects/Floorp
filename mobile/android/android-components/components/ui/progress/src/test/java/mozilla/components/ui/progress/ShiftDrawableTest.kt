/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.progress

import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ShiftDrawableTest {

    @Test
    fun draw() {
        val canvas = mock(Canvas::class.java)
        val wrappedDrawable = mock(Drawable::class.java)
        val drawable = ShiftDrawable(wrappedDrawable)

        drawable.bounds = Rect(1, 2, 3, 4)
        drawable.draw(canvas)

        verify(wrappedDrawable, times(2)).draw(canvas)
        verify(canvas, times(2)).restore()
        verify(canvas).restoreToCount(any(Int::class.java))
    }
}