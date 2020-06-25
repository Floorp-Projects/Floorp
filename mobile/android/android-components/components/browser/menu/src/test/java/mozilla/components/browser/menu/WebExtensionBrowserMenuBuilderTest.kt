/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.view.LayoutInflater
import android.view.View
import android.widget.ImageButton
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.item.BackPressMenuItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.WebExtensionBrowserMenuItem
import mozilla.components.browser.menu.item.ParentBrowserMenuItem
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebExtensionBrowserMenuBuilderTest {

    @Test
    fun `WHEN there are no web extension actions THEN add-ons menu item invokes onAddonsManagerTapped`() {
        var isAddonsManagerTapped = false
        val store = BrowserStore()
        val builder = WebExtensionBrowserMenuBuilder(
            listOf(mockMenuItem(), mockMenuItem()),
            store = store,
            onAddonsManagerTapped = { isAddonsManagerTapped = true },
            appendExtensionSubMenuAtStart = true
        )

        val menu = builder.build(testContext)

        val addonsManagerItem = menu.adapter.visibleItems[0] as? BrowserMenuImageText
        val addonsManagerItemView =
            LayoutInflater.from(testContext).inflate(addonsManagerItem!!.getLayoutResource(), null)
        addonsManagerItem.bind(menu, addonsManagerItemView)
        assertFalse(isAddonsManagerTapped)
        addonsManagerItemView.performClick()
        assertTrue(isAddonsManagerTapped)
    }

    @Test
    fun `web extension sub menu add-ons manager sub menu item invokes onAddonsManagerTapped when clicked`() {
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

        val store = BrowserStore(
            BrowserState(
                extensions = extensions
            )
        )

        var isAddonsManagerTapped = false
        val builder = WebExtensionBrowserMenuBuilder(
            listOf(mockMenuItem(), mockMenuItem()),
            store = store,
            onAddonsManagerTapped = { isAddonsManagerTapped = true },
            appendExtensionSubMenuAtStart = true
        )

        val menu = builder.build(testContext)

        val parentMenuItem = menu.adapter.visibleItems[0] as? ParentBrowserMenuItem
        val subMenuItemSize = parentMenuItem!!.subMenu.adapter.visibleItems.size
        assertEquals(6, subMenuItemSize)
        val addOnsManagerMenuItem =
            parentMenuItem.subMenu.adapter.visibleItems[subMenuItemSize - 1] as? BrowserMenuImageText
        val addonsManagerItemView =
            LayoutInflater.from(testContext).inflate(addOnsManagerMenuItem!!.getLayoutResource(), null)
        addOnsManagerMenuItem.bind(menu, addonsManagerItemView)
        assertFalse(isAddonsManagerTapped)
        addonsManagerItemView.performClick()
        assertTrue(isAddonsManagerTapped)
        assertNotNull(addOnsManagerMenuItem)
    }

    @Test
    fun `web extension submenu is added at the start when appendExtensionSubMenuAtStart is true`() {
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

        val store = BrowserStore(
            BrowserState(
                extensions = extensions
            )
        )

        val builder = WebExtensionBrowserMenuBuilder(
            listOf(mockMenuItem(), mockMenuItem()),
            store = store,
            appendExtensionSubMenuAtStart = true
        )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(3, recyclerAdapter.itemCount)

        val parentMenuItem = menu.adapter.visibleItems[0] as? ParentBrowserMenuItem
        val subMenuItemSize = parentMenuItem!!.subMenu.adapter.visibleItems.size
        assertEquals(6, subMenuItemSize)
        val backMenuItem = parentMenuItem.subMenu.adapter.visibleItems[0] as? BackPressMenuItem
        val subMenuExtItemBrowserAction = parentMenuItem.subMenu.adapter.visibleItems[2] as? WebExtensionBrowserMenuItem
        val subMenuExtItemPageAction = parentMenuItem.subMenu.adapter.visibleItems[3] as? WebExtensionBrowserMenuItem
        val addOnsManagerMenuItem = parentMenuItem.subMenu.adapter.visibleItems[subMenuItemSize - 1] as? BrowserMenuImageText
        assertNotNull(backMenuItem)
        assertEquals("browser_action", subMenuExtItemBrowserAction!!.action.title)
        assertEquals("page_action", subMenuExtItemPageAction!!.action.title)
        assertNotNull(addOnsManagerMenuItem)
    }

    @Test
    fun `web extension submenu is added at the end when appendExtensionSubMenuAtStart is false`() {
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

        val store = BrowserStore(
            BrowserState(
                extensions = extensions
            )
        )

        val builder =
            WebExtensionBrowserMenuBuilder(
                listOf(mockMenuItem(), mockMenuItem()),
                store = store,
                appendExtensionSubMenuAtStart = false
            )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(3, recyclerAdapter.itemCount)

        val parentMenuItem = menu.adapter.visibleItems[recyclerAdapter.itemCount - 1] as? ParentBrowserMenuItem
        val subMenuItemSize = parentMenuItem!!.subMenu.adapter.visibleItems.size
        assertEquals(6, subMenuItemSize)
        val backMenuItem = parentMenuItem.subMenu.adapter.visibleItems[subMenuItemSize - 1] as? BackPressMenuItem
        val subMenuExtItemBrowserAction = parentMenuItem.subMenu.adapter.visibleItems[2] as? WebExtensionBrowserMenuItem
        val subMenuExtItemPageAction = parentMenuItem.subMenu.adapter.visibleItems[3] as? WebExtensionBrowserMenuItem
        val addOnsManagerMenuItem = parentMenuItem.subMenu.adapter.visibleItems[0] as? BrowserMenuImageText
        assertNotNull(backMenuItem)
        assertEquals("browser_action", subMenuExtItemBrowserAction!!.action.title)
        assertEquals("page_action", subMenuExtItemPageAction!!.action.title)
        assertNotNull(addOnsManagerMenuItem)
    }

    private fun mockMenuItem() = object : BrowserMenuItem {
        override val visible: () -> Boolean = { true }

        override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

        override fun bind(menu: BrowserMenu, view: View) {}
    }
}
