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
import mozilla.components.browser.menu.item.ParentBrowserMenuItem
import mozilla.components.browser.menu.item.WebExtensionBrowserMenuItem
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem.Companion.MAIN_EXTENSIONS_MENU_ID
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

    private val submenuPlaceholderMenuItem = WebExtensionPlaceholderMenuItem(id = MAIN_EXTENSIONS_MENU_ID)

    @Test
    fun `WHEN there are no web extension actions THEN add-ons menu item invokes onAddonsManagerTapped`() {
        var isAddonsManagerTapped = false
        val store = BrowserStore()
        val builder = WebExtensionBrowserMenuBuilder(
            listOf(mockMenuItem(), submenuPlaceholderMenuItem, mockMenuItem()),
            store = store,
            onAddonsManagerTapped = { isAddonsManagerTapped = true },
            appendExtensionSubMenuAtStart = true,
        )

        val menu = builder.build(testContext)

        val addonsManagerItem = menu.adapter.visibleItems[1] as? BrowserMenuImageText
        val addonsManagerItemView =
            LayoutInflater.from(testContext).inflate(addonsManagerItem!!.getLayoutResource(), null)
        addonsManagerItem.bind(menu, addonsManagerItemView)
        assertFalse(isAddonsManagerTapped)
        addonsManagerItemView.performClick()
        assertTrue(isAddonsManagerTapped)
    }

    @Test
    fun `GIVEN style is provided WHEN creating extension menu THEN styles should be applied to items`() {
        val browserAction = WebExtensionBrowserAction("browser_action", true, mock(), "", 0, 0) {}
        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = browserAction,
            ),
        )

        val store = BrowserStore(BrowserState(extensions = extensions))
        val style = WebExtensionBrowserMenuBuilder.Style(
            addonsManagerMenuItemDrawableRes = R.drawable.mozac_ic_extensions,
            backPressMenuItemDrawableRes = R.drawable.mozac_ic_back,
        )
        val builder = WebExtensionBrowserMenuBuilder(
            listOf(mockMenuItem()),
            store = store,
            style = style,
            appendExtensionSubMenuAtStart = true,
        )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        menu.show(anchor)

        val parentMenuItem = menu.adapter.visibleItems[0] as ParentBrowserMenuItem
        val subMenuItemIndex = parentMenuItem.subMenu.adapter.visibleItems.lastIndex
        val backPressMenuItem = parentMenuItem.subMenu.adapter.visibleItems[0] as BackPressMenuItem
        val addonsManagerItem = parentMenuItem.subMenu.adapter.visibleItems[subMenuItemIndex] as BrowserMenuImageText

        assertEquals(style.backPressMenuItemDrawableRes, backPressMenuItem.imageResource)
        assertEquals(style.webExtIconTintColorResource, backPressMenuItem.iconTintColorResource)

        assertEquals(style.webExtIconTintColorResource, addonsManagerItem.iconTintColorResource)
        assertEquals(style.webExtIconTintColorResource, addonsManagerItem.iconTintColorResource)
    }

    @Test
    fun `web extension sub menu add-ons manager sub menu item invokes onAddonsManagerTapped when clicked`() {
        val browserAction = WebExtensionBrowserAction("browser_action", true, mock(), "", 0, 0) {}
        val pageAction = WebExtensionBrowserAction("page_action", true, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = browserAction,
                pageAction = pageAction,
            ),
        )

        val store = BrowserStore(
            BrowserState(
                extensions = extensions,
            ),
        )

        var isAddonsManagerTapped = false
        val builder = WebExtensionBrowserMenuBuilder(
            listOf(mockMenuItem(), submenuPlaceholderMenuItem, mockMenuItem()),
            store = store,
            onAddonsManagerTapped = { isAddonsManagerTapped = true },
            appendExtensionSubMenuAtStart = true,
        )

        val menu = builder.build(testContext)

        val parentMenuItem = menu.adapter.visibleItems[1] as? ParentBrowserMenuItem
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
    fun `web extension submenu is added at the top when usingBottomToolbar is true with no placeholder for submenu`() {
        val browserAction = WebExtensionBrowserAction("browser_action", true, mock(), "", 0, 0) {}
        val pageAction = WebExtensionBrowserAction("page_action", true, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = browserAction,
                pageAction = pageAction,
            ),
        )

        val store = BrowserStore(
            BrowserState(
                extensions = extensions,
            ),
        )

        val builder = WebExtensionBrowserMenuBuilder(
            listOf(mockMenuItem(), mockMenuItem(), mockMenuItem()),
            store = store,
            appendExtensionSubMenuAtStart = true,
        )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(4, recyclerAdapter.itemCount)

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
    fun `web extension submenu is added at the bottom when usingBottomToolbar is false with no placeholder for submenu `() {
        val browserAction = WebExtensionBrowserAction("browser_action", true, mock(), "", 0, 0) {}
        val pageAction = WebExtensionBrowserAction("page_action", true, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = browserAction,
                pageAction = pageAction,
            ),
        )

        val store = BrowserStore(
            BrowserState(
                extensions = extensions,
            ),
        )

        val builder =
            WebExtensionBrowserMenuBuilder(
                listOf(mockMenuItem(), mockMenuItem(), mockMenuItem()),
                store = store,
                appendExtensionSubMenuAtStart = false,
            )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(4, recyclerAdapter.itemCount)

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

    @Test
    fun `web extension is moved to main menu when extension id equals WebExtensionPlaceholderMenuItem id`() {
        val promotableWebExtensionId = "promotable extension id"
        val promotableWebExtensionTitle = "promotable extension action title"
        val testIconTintColorResource = R.color.accent_material_dark

        val pageAction = WebExtensionBrowserAction("page_action", true, mock(), "", 0, 0) {}
        val pageActionPromotableWebExtension = WebExtensionBrowserAction(promotableWebExtensionTitle, true, mock(), "", 0, 0) {}

        // just 2 extensions in the extension menu
        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = null,
                pageAction = pageAction,
            ),
            promotableWebExtensionId to WebExtensionState(
                promotableWebExtensionId,
                "url",
                "name",
                true,
                browserAction = null,
                pageAction = pageActionPromotableWebExtension,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                extensions = extensions,
            ),
        )

        // 4 items initially on the main menu
        val items = listOf(
            WebExtensionPlaceholderMenuItem(
                id = promotableWebExtensionId,
                iconTintColorResource = testIconTintColorResource,
            ),
            mockMenuItem(),
            submenuPlaceholderMenuItem,
            mockMenuItem(),
        )

        val builder =
            WebExtensionBrowserMenuBuilder(
                items,
                store = store,
                appendExtensionSubMenuAtStart = false,
            )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!! as BrowserMenuAdapter
        assertNotNull(recyclerAdapter)

        // main menu should have the 4 initial items, one replaced by web extension one replaced by the extensions menu
        assertEquals(4, recyclerAdapter.itemCount)

        val replacedItem = recyclerAdapter.visibleItems[0]
        // the replaced item should be a WebExtensionBrowserMenuItem
        assertEquals("WebExtensionBrowserMenuItem", replacedItem.javaClass.simpleName)

        // the replaced item should have the action title of the WebExtensionBrowserMenuItem
        assertEquals(promotableWebExtensionTitle, (replacedItem as WebExtensionBrowserMenuItem).action.title)

        // the replaced item should have the icon tint set by placeholder
        assertEquals(testIconTintColorResource, replacedItem.iconTintColorResource)

        val parentMenuItem = menu.adapter.visibleItems[2] as? ParentBrowserMenuItem
        val subMenuItemSize = parentMenuItem!!.subMenu.adapter.visibleItems.size

        // add-ons should only have one extension, 2 dividers, 1 add-on manager item and 1 back menu item
        assertEquals(5, subMenuItemSize)

        val backMenuItem = parentMenuItem.subMenu.adapter.visibleItems[subMenuItemSize - 1] as? BackPressMenuItem
        val subMenuExtItemPageAction = parentMenuItem.subMenu.adapter.visibleItems[2] as? WebExtensionBrowserMenuItem
        val addOnsManagerMenuItem = parentMenuItem.subMenu.adapter.visibleItems[0] as? BrowserMenuImageText

        assertNotNull(backMenuItem)
        assertEquals("page_action", subMenuExtItemPageAction!!.action.title)
        assertNotNull(addOnsManagerMenuItem)
    }

    @Test
    fun `GIVEN a placeholder with the id MAIN_EXTENSIONS_MENU_ID WHEN the menu is built THEN the extensions sub-menu is inserted in its place`() {
        val pageAction = WebExtensionBrowserAction("page_action", true, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = null,
                pageAction = pageAction,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                extensions = extensions,
            ),
        )

        // 3 items initially on the main menu
        val items = listOf(
            mockMenuItem(),
            submenuPlaceholderMenuItem,
            mockMenuItem(),
        )

        val builder =
            WebExtensionBrowserMenuBuilder(
                items,
                store = store,
                appendExtensionSubMenuAtStart = false,
            )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!! as BrowserMenuAdapter
        assertNotNull(recyclerAdapter)

        // main menu should have the 3 initial items, one replaced by extensions sub-menu
        assertEquals(3, recyclerAdapter.itemCount)

        val parentMenuItem = recyclerAdapter.visibleItems[1] as ParentBrowserMenuItem
        // the replaced item should be a ParentBrowserMenuItem
        assertEquals("ParentBrowserMenuItem", parentMenuItem.javaClass.simpleName)

        // the replaced item should have the action title of the WebExtensionBrowserMenuItem
        assertEquals("Add-ons", parentMenuItem.label)

        val subMenuItemSize = parentMenuItem.subMenu.adapter.visibleItems.size

        // add-ons should only have one extension, 2 dividers, 1 add-on manager item and 1 back menu item
        assertEquals(5, subMenuItemSize)

        val backMenuItem = parentMenuItem.subMenu.adapter.visibleItems[subMenuItemSize - 1] as? BackPressMenuItem
        val subMenuExtItemPageAction = parentMenuItem.subMenu.adapter.visibleItems[2] as? WebExtensionBrowserMenuItem
        val addOnsManagerMenuItem = parentMenuItem.subMenu.adapter.visibleItems[0] as? BrowserMenuImageText

        assertNotNull(backMenuItem)
        assertEquals("page_action", subMenuExtItemPageAction!!.action.title)
        assertNotNull(addOnsManagerMenuItem)
    }

    @Test
    fun `GIVEN showAddonsInMenu with value true WHEN the menu is built THEN the Add-ons item is added at the bottom`() {
        val pageAction = WebExtensionBrowserAction("page_action", true, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = null,
                pageAction = pageAction,
            ),
        )
        val store = BrowserStore(BrowserState(extensions = extensions))

        // 2 items initially on the main menu
        val items = listOf(
            mockMenuItem(),
            mockMenuItem(),
        )

        val builder =
            WebExtensionBrowserMenuBuilder(
                items,
                store = store,
                showAddonsInMenu = true,
            )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter as BrowserMenuAdapter
        assertNotNull(recyclerAdapter)

        // main menu should have 3 items and the last one should be the "Add-ons" item
        assertEquals(3, recyclerAdapter.itemCount)

        val lastItem = recyclerAdapter.visibleItems[2]
        assert(lastItem is ParentBrowserMenuItem && lastItem.label == "Add-ons")
    }

    @Test
    fun `GIVEN showAddonsInMenu with value false WHEN the menu is built THEN the Add-ons item is not added`() {
        val pageAction = WebExtensionBrowserAction("page_action", true, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = null,
                pageAction = pageAction,
            ),
        )
        val store = BrowserStore(BrowserState(extensions = extensions))

        // 2 items initially on the main menu
        val items = listOf(
            mockMenuItem(),
            mockMenuItem(),
        )

        val builder =
            WebExtensionBrowserMenuBuilder(
                items,
                store = store,
                showAddonsInMenu = false,
            )

        val menu = builder.build(testContext)
        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter as BrowserMenuAdapter
        assertNotNull(recyclerAdapter)

        // main menu should have 2 items
        assertEquals(2, recyclerAdapter.itemCount)

        recyclerAdapter.visibleItems.forEach { item ->
            assert(item !is ParentBrowserMenuItem)
        }
    }

    private fun mockMenuItem() = object : BrowserMenuItem {
        override val visible: () -> Boolean = { true }

        override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

        override fun bind(menu: BrowserMenu, view: View) {}
    }
}
