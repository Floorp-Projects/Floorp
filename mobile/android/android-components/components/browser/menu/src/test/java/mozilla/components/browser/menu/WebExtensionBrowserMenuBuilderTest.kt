/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.view.View
import android.widget.ImageButton
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.item.WebExtensionBrowserMenuItem
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito

@RunWith(AndroidJUnit4::class)
class WebExtensionBrowserMenuBuilderTest {

    @Test
    fun `web extension is added at the start when appendExtensionActionAtStart is true`() {
        val browserAction = WebExtensionBrowserAction("browser_action", false, mock(), "", 0, 0) {}
        val pageAction = WebExtensionBrowserAction("page_action", false, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = browserAction,
                pageAction = pageAction
            )
        )

        val store = Mockito.spy(
            BrowserStore(
                BrowserState(
                    extensions = extensions
                )
            )
        )

        val builder =
            WebExtensionBrowserMenuBuilder(
                listOf(mockMenuItem(), mockMenuItem()),
                store = store,
                appendExtensionActionAtStart = true)

        val menu = builder.build(testContext)

        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(4, recyclerAdapter.itemCount)

        var menuItem = menu.adapter.visibleItems[0]
        assertEquals("browser_action", (menuItem as WebExtensionBrowserMenuItem).action.title)

        menuItem = menu.adapter.visibleItems[1]
        assertEquals("page_action", (menuItem as WebExtensionBrowserMenuItem).action.title)
    }

    @Test
    fun `web extension is added at the end when appendExtensionActionAtStart is false`() {
        val browserAction = WebExtensionBrowserAction("browser_action", false, mock(), "", 0, 0) {}
        val pageAction = WebExtensionBrowserAction("page_action", false, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = browserAction,
                pageAction = pageAction
            )
        )

        val store = Mockito.spy(
            BrowserStore(
                BrowserState(
                    extensions = extensions
                )
            )
        )

        val builder =
            WebExtensionBrowserMenuBuilder(
                listOf(mockMenuItem(), mockMenuItem()),
                store = store,
                appendExtensionActionAtStart = false
            )

        val menu = builder.build(testContext)

        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(4, recyclerAdapter.itemCount)

        var menuItem = menu.adapter.visibleItems[menu.adapter.itemCount - 1]
        assertEquals("page_action", (menuItem as WebExtensionBrowserMenuItem).action.title)
        menuItem = menu.adapter.visibleItems[menu.adapter.itemCount - 2]
        assertEquals("browser_action", (menuItem as WebExtensionBrowserMenuItem).action.title)
    }

    private fun mockMenuItem() = object : BrowserMenuItem {
        override val visible: () -> Boolean = { true }

        override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

        override fun bind(menu: BrowserMenu, view: View) {}
    }
}
