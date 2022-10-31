/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr.views

import android.view.View
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AutoFitTextureViewTest {

    @Test
    fun `set aspect ratio`() {
        val view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(16, 9)

        assertEquals(16, view.mRatioWidth)
        assertEquals(9, view.mRatioHeight)
        verify(view).requestLayout()
    }

    @Test(expected = IllegalArgumentException::class)
    fun `width must not be negative when setting aspect ratio`() {
        val view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(-1, 0)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `height must not be negative when setting aspect ratio`() {
        val view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(0, -1)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `width and height must not be negative when setting aspect ratio`() {
        val view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(-1, -1)
    }

    @Test
    fun `measure`() {
        val width = View.MeasureSpec.getSize(640)
        val height = View.MeasureSpec.getSize(480)

        var view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(0, 0)
        view.measure(width, height)
        assertEquals(width, view.measuredWidth)
        assertEquals(height, view.measuredHeight)

        view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(640, 0)
        view.measure(width, height)
        assertEquals(width, view.measuredWidth)
        assertEquals(height, view.measuredHeight)

        view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(0, 480)
        view.measure(width, height)
        assertEquals(width, view.measuredWidth)
        assertEquals(height, view.measuredHeight)

        view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(16, 9)
        view.measure(width, height)
        assertEquals(width, view.measuredWidth)
        assertEquals(360, view.measuredHeight)

        view = spy(AutoFitTextureView(ApplicationProvider.getApplicationContext()))
        view.setAspectRatio(4, 3)
        view.measure(width, height)
        assertEquals(width, view.measuredWidth)
        assertEquals(height, view.measuredHeight)
    }
}
