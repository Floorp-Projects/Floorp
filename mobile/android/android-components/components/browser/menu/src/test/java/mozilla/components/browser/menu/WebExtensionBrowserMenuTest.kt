/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.graphics.Bitmap
import android.graphics.Color
import android.widget.Button
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.menu.WebExtensionBrowserMenu.Companion.getOrUpdateWebExtensionMenuItems
import mozilla.components.browser.menu.WebExtensionBrowserMenu.Companion.webExtensionBrowserActions
import mozilla.components.browser.menu.WebExtensionBrowserMenu.Companion.webExtensionPageActions
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.menu.item.WebExtensionBrowserMenuItem
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.concept.engine.webextension.WebExtensionPageAction
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@kotlinx.coroutines.ExperimentalCoroutinesApi
class WebExtensionBrowserMenuTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        webExtensionBrowserActions.clear()
        webExtensionPageActions.clear()
    }

    @Test
    fun `actions are only updated when the menu is shown`() {
        webExtensionBrowserActions.clear()
        val browserAction = WebExtensionBrowserAction("browser_action", false, mock(), "", 0, 0) {}
        val pageAction = WebExtensionPageAction("browser_action", false, mock(), "", 0, 0) {}
        val extensions = mapOf(
            "browser_action" to WebExtensionState(
                "browser_action",
                "url",
                "name",
                true,
                browserAction = browserAction,
                pageAction = pageAction
            )
        )

        val store =
            BrowserStore(
                BrowserState(
                    extensions = extensions
                )
            )
        val items = listOf(
            SimpleBrowserMenuItem("Hello") {},
            SimpleBrowserMenuItem("World") {}
        )

        val adapter = BrowserMenuAdapter(testContext, items)
        val menu = WebExtensionBrowserMenu(adapter, store)

        testDispatcher.advanceUntilIdle()

        val anchor = Button(testContext)
        val popup = menu.show(anchor)

        assertNotNull(popup)

        val defaultBrowserAction =
            WebExtensionBrowserAction("default_title", false, mock(), "", 0, 0) {}
        val defaultPageAction =
            WebExtensionPageAction("default_title", false, mock(), "", 0, 0) {}
        val defaultExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = defaultBrowserAction,
                pageAction = defaultPageAction
            )
        )

        createTab(
            "https://www.example.org", id = "tab1",
            extensions = defaultExtensions
        )
        assertEquals(1, webExtensionBrowserActions.size)
        assertEquals(1, webExtensionPageActions.size)

        menu.dismiss()
        val anotherBrowserAction =
                WebExtensionBrowserAction("another_title", false, mock(), "", 0, 0) {}
        val anotherPageAction =
            WebExtensionBrowserAction("another_title", false, mock(), "", 0, 0) {}
        val anotherExtension: Map<String, WebExtensionState> = mapOf(
            "id2" to WebExtensionState(
                "id2",
                "url",
                "name",
                true,
                browserAction = anotherBrowserAction,
                pageAction = anotherPageAction
            )
        )

        createTab(
            "https://www.example2.org", id = "tab2",
            extensions = anotherExtension
        )
        assertEquals(0, webExtensionBrowserActions.size)
        assertEquals(0, webExtensionPageActions.size)
    }

    @Test
    fun `render web extension actions from browser state`() {

        val defaultBrowserAction =
            WebExtensionBrowserAction("default_browser_action_title", false, mock(), "", 0, 0) {}
        val defaultPageAction =
            WebExtensionPageAction("default_page_action_title", false, mock(), "", 0, 0) {}
        val overriddenBrowserAction =
            WebExtensionBrowserAction("overridden_browser_action_title", false, mock(), "", 0, 0) {}
        val overriddenPageAction =
            WebExtensionBrowserAction("overridden_page_action_title", false, mock(), "", 0, 0) {}

        val extensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = defaultBrowserAction,
                pageAction = defaultPageAction
            )
        )
        val overriddenExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState(
                "id",
                "url",
                "name",
                true,
                browserAction = overriddenBrowserAction,
                pageAction = overriddenPageAction
            )
        )
        val store =
            BrowserStore(
                BrowserState(
                    tabs = listOf(
                        createTab(
                            "https://www.example.org", id = "tab1",
                            extensions = overriddenExtensions
                        )
                    ), selectedTabId = "tab1",
                    extensions = extensions
                )
            )

        val browserMenuItems = getOrUpdateWebExtensionMenuItems(store.state, store.state.selectedTab)
        assertEquals(2, browserMenuItems.size)

        var actionMenu = browserMenuItems[0]
        assertEquals("overridden_browser_action_title", (actionMenu as WebExtensionBrowserMenuItem).action.title)

        actionMenu = browserMenuItems[1]
        assertEquals("overridden_page_action_title", (actionMenu as WebExtensionBrowserMenuItem).action.title)
    }

    @Test
    fun `getOrUpdateWebExtensionMenuItems does not include actions from disabled extensions`() {
        val enabledPageAction =
            WebExtensionBrowserAction("enabled_page_action", false, mock(), "", 0, 0) {}
        val disabledPageAction =
            WebExtensionBrowserAction("disabled_page_action", false, mock(), "", 0, 0) {}
        val enabledBrowserAction =
            WebExtensionBrowserAction("enabled_browser_action", false, mock(), "", 0, 0) {}
        val disabledBrowserAction =
            WebExtensionBrowserAction("disabled_browser_action", false, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "enabled" to WebExtensionState(
                "enabled",
                "url",
                "name",
                true,
                browserAction = enabledBrowserAction,
                pageAction = enabledPageAction
            ),
            "disabled" to WebExtensionState(
                "disabled",
                "url",
                "name",
                false,
                browserAction = disabledBrowserAction,
                pageAction = disabledPageAction
            )
        )

        val store =
            BrowserStore(
                BrowserState(
                    extensions = extensions
                )
            )

        val browserMenuItems = getOrUpdateWebExtensionMenuItems(store.state)
        assertEquals(2, browserMenuItems.size)

        var menuAction = browserMenuItems[0]
        assertEquals(
            "enabled_browser_action",
            (menuAction as WebExtensionBrowserMenuItem).action.title
        )
        menuAction = browserMenuItems[1]
        assertEquals(
            "enabled_page_action",
            (menuAction as WebExtensionBrowserMenuItem).action.title
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
            badgeBackgroundColor = Color.BLUE
        ) {}

        val pageActionOverride = Action(
            title = "updatedTitle",
            loadIcon = null,
            enabled = false,
            badgeText = "updatedText",
            badgeTextColor = Color.RED,
            badgeBackgroundColor = Color.GREEN
        ) {}

        val browserAction = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE
        ) {}

        val browserActionOverride = Action(
            title = "updatedTitle",
            loadIcon = null,
            enabled = false,
            badgeText = "updatedText",
            badgeTextColor = Color.RED,
            badgeBackgroundColor = Color.GREEN
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
            pageAction = pageActionOverride
        )

        val tabSessionState = TabSessionState(
            content = mock(),
            extensionState = tabExtensions
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
            badgeBackgroundColor = Color.BLUE
        ) {}

        val actionExt2 = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE
        ) {}

        val browserExtensions = HashMap<String, WebExtensionState>()
        browserExtensions["1"] = WebExtensionState(id = "1", name = "extensionA", browserAction = actionExt1)
        browserExtensions["2"] = WebExtensionState(id = "2", name = "extensionB", browserAction = actionExt2)

        val tabSessionState = TabSessionState(
            content = mock(),
            extensionState = emptyMap()
        )

        val browserState = BrowserState(extensions = browserExtensions)
        val actionItems = getOrUpdateWebExtensionMenuItems(browserState, tabSessionState)
        assertEquals(2, actionItems.size)
        assertEquals(actionExt1, (actionItems[0] as WebExtensionBrowserMenuItem).action)
        assertEquals(actionExt2, (actionItems[1] as WebExtensionBrowserMenuItem).action)
    }
}
