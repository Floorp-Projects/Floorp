/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

import android.widget.LinearLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.android.Padding
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ActionButtonTest {

    @Test
    fun `set padding`() {
        var button = Toolbar.ActionButton(mock(), "imageResource") {}
        val linearLayout = LinearLayout(testContext)
        var view = button.createView(linearLayout)

        assertEquals(view.paddingLeft, 0)
        assertEquals(view.paddingTop, 0)
        assertEquals(view.paddingRight, 0)
        assertEquals(view.paddingBottom, 0)

        button = Toolbar.ActionButton(
            mock(),
            "imageResource",
            padding = Padding(16, 20, 24, 28),
        ) {}

        view = button.createView(linearLayout)
        view.paddingLeft
        assertEquals(view.paddingLeft, 16)
        assertEquals(view.paddingTop, 20)
        assertEquals(view.paddingRight, 24)
        assertEquals(view.paddingBottom, 28)
    }

    @Test
    fun `constructor with drawables`() {
        val visibilityListener = { false }
        val button = Toolbar.ActionButton(mock(), "image", visibilityListener, { false }, 0, null) { }
        assertNotNull(button.imageDrawable)
        assertEquals("image", button.contentDescription)
        assertEquals(visibilityListener, button.visible)
        assertEquals(Unit, button.bind(mock()))

        val buttonVisibility = Toolbar.ActionButton(mock(), "image") {}
        assertEquals(true, buttonVisibility.visible())
    }
}
