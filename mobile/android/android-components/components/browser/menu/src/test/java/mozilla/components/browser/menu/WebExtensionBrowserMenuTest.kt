/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.graphics.Bitmap
import android.graphics.Color
import android.view.View
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.WebExtensionBrowserMenu.Companion.getOrUpdateWebExtensionMenuItems
import mozilla.components.browser.menu.WebExtensionBrowserMenu.Companion.webExtensionBrowserActions
import mozilla.components.browser.menu.WebExtensionBrowserMenu.Companion.webExtensionPageActions
import mozilla.components.browser.menu.facts.BrowserMenuFacts.Items.WEB_EXTENSION_MENU_ITEM
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.concept.engine.webextension.WebExtensionPageAction
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import mozilla.components.support.base.facts.Action as FactsAction

@RunWith(AndroidJUnit4::class)
@kotlinx.coroutines.ExperimentalCoroutinesApi
class WebExtensionBrowserMenuTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        webExtensionBrowserActions.clear()
        webExtensionPageActions.clear()
    }

    @Test
    fun `actions are only updated when the menu is shown`() {
        webExtensionBrowserActions.clear()
        val browserAction = WebExtensionBrowserAction("browser_action", true, mock(), "", 0, 0) {}
        val pageAction = WebExtensionPageAction("browser_action", true, mock(), "", 0, 0) {}
        val extensions = mapOf(
            "browser_action" to WebExtensionState(
                "browser_action",
                "url",
                "name",
                true,
                browserAction = browserAction,
                pageAction = pageAction,
            ),
        )

        val store =
            BrowserStore(
                BrowserState(
                    extensions = extensions,
                ),
            )
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {},
        )

        val adapter = BrowserMenuAdapter(testContext, items)
        val menu = WebExtensionBrowserMenu(adapter, store)

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertNotNull(popup)

        val defaultBrowserAction =
            WebExtensionBrowserAction("default_title", true, mock(), "", 0, 0) {}
        val defaultPageAction =
            WebExtensionPageAction("default_title", true, mock(), "", 0, 0) {}
        val defaultExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = defaultBrowserAction,
                pageAction = defaultPageAction,
            ),
        )

        createTab(
            "https://www.example.org",
            id = "tab1",
            extensions = defaultExtensions,
        )
        assertEquals(1, webExtensionBrowserActions.size)
        assertEquals(1, webExtensionPageActions.size)

        menu.dismiss()
        val anotherBrowserAction =
            WebExtensionBrowserAction("another_title", true, mock(), "", 0, 0) {}
        val anotherPageAction =
            WebExtensionBrowserAction("another_title", true, mock(), "", 0, 0) {}
        val anotherExtension: Map<String, WebExtensionState> = mapOf(
            "id2" to WebExtensionState(
                "id2",
                "url",
                "name",
                true,
                browserAction = anotherBrowserAction,
                pageAction = anotherPageAction,
            ),
        )

        createTab(
            "https://www.example2.org",
            id = "tab2",
            extensions = anotherExtension,
        )
        assertEquals(0, webExtensionBrowserActions.size)
        assertEquals(0, webExtensionPageActions.size)
    }

    @Test
    fun `render web extension actions from browser state`() {
        val defaultBrowserAction =
            WebExtensionBrowserAction("default_browser_action_title", true, mock(), "", 0, 0) {}
        val defaultPageAction =
            WebExtensionPageAction("default_page_action_title", true, mock(), "", 0, 0) {}
        val overriddenBrowserAction =
            WebExtensionBrowserAction("overridden_browser_action_title", true, mock(), "", 0, 0) {}
        val overriddenPageAction =
            WebExtensionBrowserAction("overridden_page_action_title", true, mock(), "", 0, 0) {}

        val extensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = defaultBrowserAction,
                pageAction = defaultPageAction,
            ),
        )
        val overriddenExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = overriddenBrowserAction,
                pageAction = overriddenPageAction,
            ),
        )
        val store =
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(
                            "https://www.example.org",
                            id = "tab1",
                            extensions = overriddenExtensions,
                        ),
                    ),
                    selectedTabId = "tab1",
                    extensions = extensions,
                ),
            )

        val browserMenuItems =
            getOrUpdateWebExtensionMenuItems(store.state, store.state.selectedTab)
        assertEquals(2, browserMenuItems.size)

        var actionMenu = browserMenuItems[0]
        assertEquals(
            "overridden_browser_action_title",
            actionMenu.action.title,
        )

        actionMenu = browserMenuItems[1]
        assertEquals(
            "overridden_page_action_title",
            actionMenu.action.title,
        )
    }

    @Test
    fun `getOrUpdateWebExtensionMenuItems does not include actions from disabled extensions`() {
        val enabledPageAction =
            WebExtensionBrowserAction("enabled_page_action", true, mock(), "", 0, 0) {}
        val disabledPageAction =
            WebExtensionBrowserAction("disabled_page_action", true, mock(), "", 0, 0) {}
        val enabledBrowserAction =
            WebExtensionBrowserAction("enabled_browser_action", true, mock(), "", 0, 0) {}
        val disabledBrowserAction =
            WebExtensionBrowserAction("disabled_browser_action", true, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "enabled" to WebExtensionState(
                "enabled",
                "url",
                "name",
                true,
                browserAction = enabledBrowserAction,
                pageAction = enabledPageAction,
            ),
            "disabled" to WebExtensionState(
                "disabled",
                "url",
                "name",
                false,
                browserAction = disabledBrowserAction,
                pageAction = disabledPageAction,
            ),
        )

        val store =
            BrowserStore(
                BrowserState(
                    extensions = extensions,
                ),
            )

        val browserMenuItems = getOrUpdateWebExtensionMenuItems(store.state)
        assertEquals(2, browserMenuItems.size)

        var menuAction = browserMenuItems[0]
        assertEquals(
            "enabled_browser_action",
            menuAction.action.title,
        )
        menuAction = browserMenuItems[1]
        assertEquals(
            "enabled_page_action",
            menuAction.action.title,
        )
    }

    @Test
    fun `browser actions can be overridden per tab`() {
        webExtensionBrowserActions.clear()
        val loadIcon: (suspend (Int) -> Bitmap?)? = { mock() }
        val pageAction = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val pageActionOverride = Action(
            title = "updatedTitle",
            loadIcon = null,
            enabled = false,
            badgeText = "updatedText",
            badgeTextColor = Color.RED,
            badgeBackgroundColor = Color.GREEN,
        ) {}

        val browserAction = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val browserActionOverride = Action(
            title = "updatedTitle",
            loadIcon = null,
            enabled = false,
            badgeText = "updatedText",
            badgeTextColor = Color.RED,
            badgeBackgroundColor = Color.GREEN,
        ) {}

        val browserExtensions = HashMap<String, WebExtensionState>()
        browserExtensions["1"] =
            WebExtensionState(id = "1", browserAction = browserAction, pageAction = pageAction)

        val browserState = BrowserState(extensions = browserExtensions)
        getOrUpdateWebExtensionMenuItems(browserState, mock())

        // Verifying global browser action
        assertTrue(webExtensionBrowserActions.size == 1)
        var ext1 = webExtensionBrowserActions["1"]
        assertTrue(ext1?.action?.enabled!!)
        assertEquals("badgeText", ext1.action.badgeText!!)
        assertEquals("title", ext1.action.title!!)
        assertEquals(loadIcon, ext1.action.loadIcon!!)
        assertEquals(Color.WHITE, ext1.action.badgeTextColor!!)
        assertEquals(Color.BLUE, ext1.action.badgeBackgroundColor!!)

        // Verifying global page action
        assertTrue(webExtensionPageActions.size == 1)
        ext1 = webExtensionPageActions["1"]!!
        assertTrue(ext1.action.enabled!!)
        assertEquals("badgeText", ext1.action.badgeText!!)
        assertEquals("title", ext1.action.title!!)
        assertEquals(loadIcon, ext1.action.loadIcon!!)
        assertEquals(Color.WHITE, ext1.action.badgeTextColor!!)
        assertEquals(Color.BLUE, ext1.action.badgeBackgroundColor!!)

        val tabExtensions = HashMap<String, WebExtensionState>()
        tabExtensions["1"] = WebExtensionState(
            id = "1",
            browserAction = browserActionOverride,
            pageAction = pageActionOverride,
        )

        val tabSessionState = TabSessionState(
            content = mock(),
            extensionState = tabExtensions,
        )

        getOrUpdateWebExtensionMenuItems(browserState, tabSessionState)

        // Verify rendering session-specific browser action override
        assertTrue(webExtensionBrowserActions.size == 1)
        var updatedExt1 = webExtensionBrowserActions["1"]
        assertFalse(updatedExt1?.action?.enabled!!)
        assertEquals("updatedText", updatedExt1.action.badgeText!!)
        assertEquals("updatedTitle", updatedExt1.action.title!!)
        assertEquals(loadIcon, updatedExt1.action.loadIcon!!)
        assertEquals(Color.RED, updatedExt1.action.badgeTextColor!!)
        assertEquals(Color.GREEN, updatedExt1.action.badgeBackgroundColor!!)

        // Verify rendering session-specific page action override
        assertTrue(webExtensionPageActions.size == 1)
        updatedExt1 = webExtensionBrowserActions["1"]!!
        assertFalse(updatedExt1.action.enabled!!)
        assertEquals("updatedText", updatedExt1.action.badgeText!!)
        assertEquals("updatedTitle", updatedExt1.action.title!!)
        assertEquals(loadIcon, updatedExt1.action.loadIcon!!)
        assertEquals(Color.RED, updatedExt1.action.badgeTextColor!!)
        assertEquals(Color.GREEN, updatedExt1.action.badgeBackgroundColor!!)
    }

    @Test
    fun `actions are sorted per extension name`() {
        val loadIcon: (suspend (Int) -> Bitmap?)? = { mock() }

        val actionExt1 = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val actionExt2 = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val browserExtensions = HashMap<String, WebExtensionState>()
        browserExtensions["1"] =
            WebExtensionState(id = "1", name = "extensionA", browserAction = actionExt1)
        browserExtensions["2"] =
            WebExtensionState(id = "2", name = "extensionB", browserAction = actionExt2)

        val tabSessionState = TabSessionState(
            content = mock(),
            extensionState = emptyMap(),
        )

        val browserState = BrowserState(extensions = browserExtensions)
        val actionItems = getOrUpdateWebExtensionMenuItems(browserState, tabSessionState)
        assertEquals(2, actionItems.size)
        assertEquals(actionExt1, actionItems[0].action)
        assertEquals(actionExt2, actionItems[1].action)
    }

    @Test
    fun `clicking on the menu item should emit a BrowserMenuFacts with the web extension id`() {
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView = TextView(testContext)
        val container = View(testContext)
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.findViewById<View>(R.id.container)).thenReturn(container)
        whenever(view.context).thenReturn(mock())

        val browserAction =
            WebExtensionBrowserAction("title", true, mock(), "", 0, 0) {}
        val pageAction =
            WebExtensionPageAction("title", true, mock(), "", 0, 0) {}
        val extensions: Map<String, WebExtensionState> = mapOf(
            "some_example_id" to WebExtensionState(
                "some_example_id",
                "url",
                "name",
                true,
                browserAction = browserAction,
                pageAction = pageAction,
            ),
        )

        val store =
            BrowserStore(
                BrowserState(
                    extensions = extensions,
                ),
            )

        val browserMenuItems = getOrUpdateWebExtensionMenuItems(store.state)
        val menuItem = browserMenuItems[1]
        val menu: WebExtensionBrowserMenu = mock()

        menuItem.bind(menu, view)

        CollectionProcessor.withFactCollection { facts ->
            container.performClick()

            val fact = facts[0]
            assertEquals(FactsAction.CLICK, fact.action)
            assertEquals(WEB_EXTENSION_MENU_ITEM, fact.item)
            assertEquals(1, fact.metadata?.size)
            assertTrue(fact.metadata?.containsKey("id")!!)
            assertEquals("some_example_id", fact.metadata?.get("id"))
        }
    }

    @Test
    fun `hides browser and page actions in private tabs if extension is not allowed to run`() {
        val loadIcon: (suspend (Int) -> Bitmap?)? = { mock() }

        val actionExt1 = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val tabSessionState = TabSessionState(
            content = mock(),
            extensionState = emptyMap(),
        )
        whenever(tabSessionState.content.private).thenReturn(true)

        val browserExtensions = HashMap<String, WebExtensionState>()
        browserExtensions["1"] =
            WebExtensionState(id = "1", name = "extensionA", browserAction = actionExt1)
        val browserState = BrowserState(extensions = browserExtensions)
        val actionItems = getOrUpdateWebExtensionMenuItems(browserState, tabSessionState)
        assertEquals(0, actionItems.size)

        val browserExtensionsAllowedInPrivateBrowsing = HashMap<String, WebExtensionState>()
        browserExtensionsAllowedInPrivateBrowsing["1"] =
            WebExtensionState(id = "1", allowedInPrivateBrowsing = true, name = "extensionA", browserAction = actionExt1)
        val browserStateAllowedInPrivateBrowsing = BrowserState(extensions = browserExtensionsAllowedInPrivateBrowsing)
        val actionItemsAllowedInPrivateBrowsing = getOrUpdateWebExtensionMenuItems(browserStateAllowedInPrivateBrowsing, tabSessionState)
        assertEquals(1, actionItemsAllowedInPrivateBrowsing.size)
        assertEquals(actionExt1, actionItemsAllowedInPrivateBrowsing[0].action)
    }

    @Test
    fun `does not include menu item for disabled paged actions`() {
        val enabledPageAction =
            WebExtensionBrowserAction("enabled_page_action", true, mock(), "", 0, 0) {}
        val disabledPageAction =
            WebExtensionBrowserAction("disabled_page_action", false, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "ext1" to WebExtensionState(
                "ext1",
                "url",
                "name",
                true,
                pageAction = enabledPageAction,
            ),
            "ext2" to WebExtensionState(
                "ext2",
                "url",
                "name",
                true,
                pageAction = disabledPageAction,
            ),
        )

        val store =
            BrowserStore(
                BrowserState(
                    extensions = extensions,
                ),
            )

        val browserMenuItems = getOrUpdateWebExtensionMenuItems(store.state)
        assertEquals(1, browserMenuItems.size)

        var menuAction = browserMenuItems[0]
        assertEquals(
            "enabled_page_action",
            menuAction.action.title,
        )
    }
}
