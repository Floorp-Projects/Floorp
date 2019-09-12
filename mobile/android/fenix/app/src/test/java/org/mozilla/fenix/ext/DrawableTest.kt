package org.mozilla.fenix.ext

import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.mozilla.fenix.TestApplication
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import android.graphics.drawable.Drawable
import android.graphics.Rect
import android.graphics.Canvas
import android.graphics.ColorFilter

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)

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
        override fun setColorFilter(cf: ColorFilter) {}
        override fun onBoundsChange(bounds: Rect) {
            boundsChanged = true
            super.onBoundsChange(bounds)
        }
    }
}
