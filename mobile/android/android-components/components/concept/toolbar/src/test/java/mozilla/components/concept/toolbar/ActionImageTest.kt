/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

import android.graphics.drawable.Drawable
import android.view.View
import android.view.ViewGroup
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.android.Padding
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class ActionImageTest {

    @Test
    fun `setting minimumWidth`() {
        val drawable: Drawable = mock()
        val image = Toolbar.ActionImage(drawable)
        val emptyImage = Toolbar.ActionImage(mock())

        val viewGroup: ViewGroup = mock()
        `when`(viewGroup.context).thenReturn(testContext)
        `when`(drawable.intrinsicWidth).thenReturn(5)

        val emptyImageView = emptyImage.createView(viewGroup)
        assertEquals(0, emptyImageView.minimumWidth)

        val imageView = image.createView(viewGroup)
        assertTrue(imageView.minimumWidth != 0)
    }

    @Test
    fun `accessibility description provided`() {
        val image = Toolbar.ActionImage(mock())
        var imageAccessible = Toolbar.ActionImage(mock(), "image")
        val viewGroup: ViewGroup = mock()
        `when`(viewGroup.context).thenReturn(testContext)

        val imageView = image.createView(viewGroup)
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, imageView.importantForAccessibility)

        var imageViewAccessible = imageAccessible.createView(viewGroup)
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO, imageViewAccessible.importantForAccessibility)

        imageAccessible = Toolbar.ActionImage(mock(), "")
        imageViewAccessible = imageAccessible.createView(viewGroup)
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, imageViewAccessible.importantForAccessibility)
    }

    @Test
    fun `bind is not implemented`() {
        val button = Toolbar.ActionImage(mock())
        assertEquals(Unit, button.bind(mock()))
    }

    @Test
    fun `padding is set`() {
        var image = Toolbar.ActionImage(mock())
        val viewGroup: ViewGroup = mock()
        `when`(viewGroup.context).thenReturn(testContext)
        var view = image.createView(viewGroup)

        assertEquals(view.paddingLeft, 0)
        assertEquals(view.paddingTop, 0)
        assertEquals(view.paddingRight, 0)
        assertEquals(view.paddingBottom, 0)

        image = Toolbar.ActionImage(mock(), padding = Padding(16, 20, 24, 28))

        view = image.createView(viewGroup)
        assertEquals(view.paddingLeft, 16)
        assertEquals(view.paddingTop, 20)
        assertEquals(view.paddingRight, 24)
        assertEquals(view.paddingBottom, 28)
    }
}
