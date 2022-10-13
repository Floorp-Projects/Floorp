/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2

import android.widget.Button
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BrowserMenuControllerTest {

    @Test
    fun `created popup window is displayed automatically`() {
        val menu: MenuController = BrowserMenuController()

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)
    }

    @Test
    fun `observer is notified when submitList is called`() {
        var submitted: List<MenuCandidate>? = null
        val menu: MenuController = BrowserMenuController()
        menu.register(
            object : MenuController.Observer {
                override fun onMenuListSubmit(list: List<MenuCandidate>) {
                    submitted = list
                }

                override fun onDismiss() = Unit
            },
        )

        assertNull(submitted)

        menu.submitList(listOf(DecorativeTextMenuCandidate("Hello")))
        assertEquals(listOf(DecorativeTextMenuCandidate("Hello")), submitted)
    }

    @Test
    fun `dismissing the browser menu will dismiss the popup`() {
        var dismissed = false
        val menu: MenuController = BrowserMenuController()
        menu.register(
            object : MenuController.Observer {
                override fun onDismiss() {
                    dismissed = true
                }

                override fun onMenuListSubmit(list: List<MenuCandidate>) = Unit
            },
        )

        menu.dismiss()
        assertFalse(dismissed)

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)
        assertFalse(dismissed)

        menu.dismiss()

        assertFalse(popup.isShowing)
        assertTrue(dismissed)
    }
}
