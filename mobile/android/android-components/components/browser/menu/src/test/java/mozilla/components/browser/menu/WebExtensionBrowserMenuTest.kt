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
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@kotlinx.coroutines.ExperimentalCoroutinesApi
class WebExtensionBrowserMenuTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Test
    fun `webExtensionBrowserActions is only updated when the menu is shown`() {
        webExtensionBrowserActions.clear()
        val browserAction = WebExtensionBrowserAction("browser_action", false, mock(), "", 0, 0) {}
        val extensions = mapOf(
            "browser_action" to WebExtensionState("browser_action", "url", true, browserAction)
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
        val defaultExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", true, defaultBrowserAction)
        )

        createTab(
            "https://www.example.org", id = "tab1",
            extensions = defaultExtensions
        )
        assertEquals(1, webExtensionBrowserActions.size)

        menu.dismiss()
        val anotherAction =
                WebExtensionBrowserAction("another_title", false, mock(), "", 0, 0) {}
        val anotherExtension: Map<String, WebExtensionState> = mapOf(
                "id2" to WebExtensionState("id2", "url", true, anotherAction)
        )

        createTab(
            "https://www.example2.org", id = "tab2",
            extensions = anotherExtension
        )
        assertEquals(0, webExtensionBrowserActions.size)
    }

    @Test
    fun `render web extension action from browser state`() {
        webExtensionBrowserActions.clear()
        val defaultBrowserAction =
            WebExtensionBrowserAction("default_title", false, mock(), "", 0, 0) {}
        val overriddenBrowserAction =
            WebExtensionBrowserAction("overridden_title", false, mock(), "", 0, 0) {}

        val extensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", true, defaultBrowserAction)
        )
        val overriddenExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", true, overriddenBrowserAction)
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
        assertEquals(1, browserMenuItems.size)

        val extension = browserMenuItems[0]
        assertEquals("overridden_title", (extension as WebExtensionBrowserMenuItem).browserAction.title)
    }

    @Test
    fun `getOrUpdateWebExtensionMenuItems does not include actions from disabled extensions`() {
        webExtensionBrowserActions.clear()
        val enabledAction = WebExtensionBrowserAction("enabled", false, mock(), "", 0, 0) {}
        val disabledAction = WebExtensionBrowserAction("disabled", false, mock(), "", 0, 0) {}

        val extensions = mapOf(
            "enabled" to WebExtensionState("enabled", "url", true, enabledAction),
            "disabled" to WebExtensionState("disabled", "url", false, disabledAction)
        )

        val store =
            BrowserStore(
                BrowserState(
                    extensions = extensions
                )
            )

        val browserMenuItems = getOrUpdateWebExtensionMenuItems(store.state)
        assertEquals(1, browserMenuItems.size)

        val extension = browserMenuItems[0]
        assertEquals("enabled", (extension as WebExtensionBrowserMenuItem).browserAction.title)
    }

    @Test
    fun `browser actions can be overridden per tab`() {
        webExtensionBrowserActions.clear()
        val loadIcon: (suspend (Int) -> Bitmap?)? = { mock() }
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

        // Verify rendering global default browser action
        val browserExtensions = HashMap<String, WebExtensionState>()
        browserExtensions["1"] = WebExtensionState(id = "1", browserAction = browserAction)

        val browserState = BrowserState(extensions = browserExtensions)
        getOrUpdateWebExtensionMenuItems(browserState, mock())

        assertTrue(webExtensionBrowserActions.size == 1)
        val ext1 = webExtensionBrowserActions["1"]
        assertTrue(ext1?.browserAction?.enabled!!)
        assertEquals("badgeText", ext1.browserAction.badgeText!!)
        assertEquals("title", ext1.browserAction.title!!)
        assertEquals(loadIcon, ext1.browserAction.loadIcon!!)
        assertEquals(Color.WHITE, ext1.browserAction.badgeTextColor!!)
        assertEquals(Color.BLUE, ext1.browserAction.badgeBackgroundColor!!)

        // Verify rendering session-specific browser action override
        val tabExtensions = HashMap<String, WebExtensionState>()
        tabExtensions["1"] = WebExtensionState(id = "1", browserAction = browserActionOverride)

        val tabSessionState = TabSessionState(
            content = mock(),
            extensionState = tabExtensions
        )

        getOrUpdateWebExtensionMenuItems(browserState, tabSessionState)

        assertTrue(webExtensionBrowserActions.size == 1)
        val updatedExt1 = webExtensionBrowserActions["1"]
        assertFalse(updatedExt1?.browserAction?.enabled!!)
        assertEquals("updatedText", updatedExt1.browserAction.badgeText!!)
        assertEquals("updatedTitle", updatedExt1.browserAction.title!!)
        assertEquals(loadIcon, updatedExt1.browserAction.loadIcon!!)
        assertEquals(Color.RED, updatedExt1.browserAction.badgeTextColor!!)
        assertEquals(Color.GREEN, updatedExt1.browserAction.badgeBackgroundColor!!)
    }
}
