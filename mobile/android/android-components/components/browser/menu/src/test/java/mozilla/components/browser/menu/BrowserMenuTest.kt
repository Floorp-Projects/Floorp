/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.content.Context
import androidx.recyclerview.widget.RecyclerView
import android.view.Gravity
import android.view.View
import android.widget.Button
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserMenuTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `show returns non-null popup window`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(context, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(context)
        val popup = menu.show(anchor)

        assertNotNull(popup)
    }

    @Test
    fun `recyclerview adapter will have items for every menu item`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(context, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(context)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(2, recyclerAdapter.itemCount)
    }

    @Test
    fun `created popup window is displayed automatically`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(context, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(context)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)
    }

    @Test
    fun `dismissing the browser menu will dismiss the popup`() {
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {})

        val adapter = BrowserMenuAdapter(context, items)

        val menu = BrowserMenu(adapter)

        val anchor = Button(context)
        val popup = menu.show(anchor)

        assertTrue(popup.isShowing)

        menu.dismiss()

        assertFalse(popup.isShowing)
    }

    @Test
    fun `determineMenuOrientation returns Orientation-DOWN by default`() {
        assertEquals(
            BrowserMenu.Orientation.DOWN,
            BrowserMenu.determineMenuOrientation(mock())
        )
    }

    @Test
    fun `determineMenuOrientation returns Orientation-UP for views with bottom gravity in CoordinatorLayout`() {
        val params = CoordinatorLayout.LayoutParams(100, 100)
        params.gravity = Gravity.BOTTOM

        val view = View(context)
        view.layoutParams = params

        assertEquals(
            BrowserMenu.Orientation.UP,
            BrowserMenu.determineMenuOrientation(view)
        )
    }

    @Test
    fun `determineMenuOrientation returns Orientation-DOWN for views with top gravity in CoordinatorLayout`() {
        val params = CoordinatorLayout.LayoutParams(100, 100)
        params.gravity = Gravity.TOP

        val view = View(context)
        view.layoutParams = params

        assertEquals(
            BrowserMenu.Orientation.DOWN,
            BrowserMenu.determineMenuOrientation(view)
        )
    }
}