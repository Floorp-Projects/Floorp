/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.toolbar

import android.widget.FrameLayout
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
class ActionToggleButtonTest {
    @Test
    fun `clicking view will toggle state`() {
        val button = Toolbar.ActionToggleButton(0, 0, UUID.randomUUID().toString(), UUID.randomUUID().toString()) {}
        val view = button.createView(FrameLayout(RuntimeEnvironment.application))

        assertFalse(button.isSelected())

        view.performClick()

        assertTrue(button.isSelected())

        view.performClick()

        assertFalse(button.isSelected())
    }

    @Test
    fun `clicking view will invoke listener`() {
        var listenerInvoked = false

        val button = Toolbar.ActionToggleButton(0, 0, UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
            listenerInvoked = true
        }

        val view = button.createView(FrameLayout(RuntimeEnvironment.application))

        assertFalse(listenerInvoked)

        view.performClick()

        assertTrue(listenerInvoked)
    }

    @Test
    fun `toggle will invoke listener`() {
        var listenerInvoked = false

        val button = Toolbar.ActionToggleButton(0, 0, UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
            listenerInvoked = true
        }

        assertFalse(listenerInvoked)

        button.toggle()

        assertTrue(listenerInvoked)
    }

    @Test
    fun `toggle will not invoke listener if notifyListener is set to false`() {
        var listenerInvoked = false

        val button = Toolbar.ActionToggleButton(0, 0, UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
            listenerInvoked = true
        }

        assertFalse(listenerInvoked)

        button.toggle(notifyListener = false)

        assertFalse(listenerInvoked)
    }

    @Test
    fun `setSelected will invoke listener`() {
        var listenerInvoked = false

        val button = Toolbar.ActionToggleButton(0, 0, UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
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

        val button = Toolbar.ActionToggleButton(0, 0, UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
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

        val button = Toolbar.ActionToggleButton(0, 0, UUID.randomUUID().toString(), UUID.randomUUID().toString()) {
            listenerInvoked = true
        }

        assertFalse(button.isSelected())
        assertFalse(listenerInvoked)

        button.setSelected(true, notifyListener = false)

        assertFalse(listenerInvoked)
    }

    @Test
    fun `isSelected will always return correct state`() {
        val button = Toolbar.ActionToggleButton(0, 0, UUID.randomUUID().toString(), UUID.randomUUID().toString()) {}
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

        val view = button.createView(FrameLayout(RuntimeEnvironment.application))
        view.performClick()
        assertTrue(button.isSelected())
    }
}