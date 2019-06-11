/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package mozilla.components.ui.doorhanger

import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DoorhangerTest {

    @Test
    fun `show returns non-null popup window`() {
        val doorhanger = Doorhanger(TextView(testContext))
        val popupWindow = doorhanger.show(View(testContext))

        assertNotNull(popupWindow)
        assertTrue(popupWindow.isShowing)
    }

    @Test
    fun `popup window contains passed in view`() {
        val view = TextView(testContext).apply {
            id = 42
            text = "Mozilla!"
        }

        val doorhanger = Doorhanger(view)
        val popupWindow = doorhanger.show(View(testContext))

        assertEquals(
            "Mozilla!",
            popupWindow.contentView.findViewById<TextView>(42).text
        )
    }

    @Test
    fun `popup window passed in view is wrapped`() {
        val view = TextView(testContext)
        val doorhanger = Doorhanger(view)
        val popupWindow = doorhanger.show(View(testContext))

        assertNotEquals(view, popupWindow.contentView)
        assertTrue(popupWindow.contentView is ViewGroup)
    }

    @Test
    fun `dismissing popup window invokes callback`() {
        var dismissCallbackInvoked = false

        val doorhanger = Doorhanger(TextView(testContext), onDismiss = {
            dismissCallbackInvoked = true
        })

        val popupWindow = doorhanger.show(View(testContext))

        assertFalse(dismissCallbackInvoked)

        popupWindow.dismiss()

        assertTrue(dismissCallbackInvoked)
    }

    @Test
    fun `dismiss on doorhanger is forwarded to popup window`() {
        val doorhanger = Doorhanger(TextView(testContext))
        val popupWindow = doorhanger.show(View(testContext))

        assertTrue(popupWindow.isShowing)

        doorhanger.dismiss()

        assertFalse(popupWindow.isShowing)
    }

    @Test
    fun `calling dismiss on a not-showing doorhanger is a no-op`() {
        val doorhanger = Doorhanger(TextView(testContext))
        doorhanger.dismiss()
    }
}
