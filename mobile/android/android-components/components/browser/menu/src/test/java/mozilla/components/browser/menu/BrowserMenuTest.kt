/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.support.v7.widget.RecyclerView
import android.widget.Button
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class BrowserMenuTest {
    @Test
    fun `show returns non-null popup window`() {
        val items = listOf(
                SimpleBrowserMenuItem("Hello") {},
                SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(RuntimeEnvironment.application, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(RuntimeEnvironment.application)
        val popup = menu.show(anchor)

        assertNotNull(popup)
    }

    @Test
    fun `recyclerview adapter will have items for every menu item`() {
        val items = listOf(
                SimpleBrowserMenuItem("Hello") {},
                SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(RuntimeEnvironment.application, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(RuntimeEnvironment.application)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter
        assertNotNull(recyclerAdapter)
        assertEquals(2, recyclerAdapter.itemCount)
    }

    @Test
    fun `created popup window is displayed automatically`() {
        val items = listOf(
                SimpleBrowserMenuItem("Hello") {},
                SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(RuntimeEnvironment.application, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(RuntimeEnvironment.application)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)
    }

    @Test
    fun `dismissing the browser menu will dismiss the popup`() {
        val items = listOf(
                SimpleBrowserMenuItem("Hello") {},
                SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(RuntimeEnvironment.application, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(RuntimeEnvironment.application)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)

        menu.dismiss()

        assertFalse(popup.isShowing)
    }
}
