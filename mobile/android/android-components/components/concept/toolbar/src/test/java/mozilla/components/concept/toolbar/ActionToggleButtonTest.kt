/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.android.Padding
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.UUID

@RunWith(AndroidJUnit4::class)
class ActionToggleButtonTest {

    @Test
    fun `clicking view will toggle state`() {
        val button =
            Toolbar.ActionToggleButton(mock(), mock(), UUID.randomUUID().toString(), UUID.randomUUID().toString()) {}
        val view = button.createView(FrameLayout(testContext))

        assertFalse(button.isSelected())

        view.performClick()

        assertTrue(button.isSelected())

        view.performClick()

        assertFalse(button.isSelected())
    }

    @Test
    fun `clicking view will invoke listener`() {
        var listenerInvoked = false

        val button =
            Toolbar.ActionToggleButton(mock(), mock(), UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
                listenerInvoked = true
            }

        val view = button.createView(FrameLayout(testContext))

        assertFalse(listenerInvoked)

        view.performClick()

        assertTrue(listenerInvoked)
    }

    @Test
    fun `toggle will invoke listener`() {
        var listenerInvoked = false

        val button =
            Toolbar.ActionToggleButton(mock(), mock(), UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
                listenerInvoked = true
            }

        assertFalse(listenerInvoked)

        button.toggle()

        assertTrue(listenerInvoked)
    }

    @Test
    fun `toggle will not invoke listener if notifyListener is set to false`() {
        var listenerInvoked = false

        val button =
            Toolbar.ActionToggleButton(mock(), mock(), UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
                listenerInvoked = true
            }

        assertFalse(listenerInvoked)

        button.toggle(notifyListener = false)

        assertFalse(listenerInvoked)
    }

    @Test
    fun `setSelected will invoke listener`() {
        var listenerInvoked = false

        val button =
            Toolbar.ActionToggleButton(mock(), mock(), UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
                listenerInvoked = true
            }

        assertFalse(button.isSelected())
        assertFalse(listenerInvoked)

        button.setSelected(true)

        assertTrue(listenerInvoked)
    }

    @Test
    fun `setSelected will not invoke listener if value has not changed`() {
        var listenerInvoked = false

        val button =
            Toolbar.ActionToggleButton(mock(), mock(), UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
                listenerInvoked = true
            }

        assertFalse(button.isSelected())
        assertFalse(listenerInvoked)

        button.setSelected(false)

        assertFalse(listenerInvoked)
    }

    @Test
    fun `setSelected will not invoke listener if notifyListener is set to false`() {
        var listenerInvoked = false

        val button =
            Toolbar.ActionToggleButton(mock(), mock(), UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
                listenerInvoked = true
            }

        assertFalse(button.isSelected())
        assertFalse(listenerInvoked)

        button.setSelected(true, notifyListener = false)

        assertFalse(listenerInvoked)
    }

    @Test
    fun `isSelected will always return correct state`() {
        val button =
            Toolbar.ActionToggleButton(mock(), mock(), UUID.randomUUID().toString(), UUID.randomUUID().toString()) {}
        assertFalse(button.isSelected())

        button.toggle()
        assertTrue(button.isSelected())

        button.setSelected(true)
        assertTrue(button.isSelected())

        button.setSelected(false)
        assertFalse(button.isSelected())

        button.setSelected(true, notifyListener = false)
        assertTrue(button.isSelected())

        button.toggle(notifyListener = false)
        assertFalse(button.isSelected())

        val view = button.createView(FrameLayout(testContext))
        view.performClick()
        assertTrue(button.isSelected())
    }

    @Test
    fun `Toolbar ActionToggleButton must set padding`() {
        var button = Toolbar.ActionToggleButton(mock(), mock(), "imageResource", "") {}
        val linearLayout = LinearLayout(testContext)
        var view = button.createView(linearLayout)
        val padding = Padding(16, 20, 24, 28)

        assertEquals(view.paddingLeft, 0)
        assertEquals(view.paddingTop, 0)
        assertEquals(view.paddingRight, 0)
        assertEquals(view.paddingBottom, 0)

        button = Toolbar.ActionToggleButton(mock(), mock(), "imageResource", "", padding = padding) {}

        view = button.createView(linearLayout)
        view.paddingLeft
        assertEquals(view.paddingLeft, 16)
        assertEquals(view.paddingTop, 20)
        assertEquals(view.paddingRight, 24)
        assertEquals(view.paddingBottom, 28)
    }

    @Test
    fun `default constructor with drawables`() {
        var selectedValue = false
        val visibility = { true }
        val button = Toolbar.ActionToggleButton(mock(), mock(), "image", "selected", visible = visibility) { value ->
            selectedValue = value
        }
        assertEquals(true, button.visible())
        assertNotNull(button.imageDrawable)
        assertNotNull(button.imageSelectedDrawable)
        assertEquals(visibility, button.visible)
        button.setSelected(true)
        assertTrue(selectedValue)

        val buttonVisibility = Toolbar.ActionToggleButton(mock(), mock(), "image", "selected", background = 0) { }
        assertTrue(buttonVisibility.visible())
    }

    @Test
    fun `bind is not implemented`() {
        val button = Toolbar.ActionToggleButton(mock(), mock(), "image", "imageSelected") {}
        assertEquals(Unit, button.bind(mock()))
    }
}
