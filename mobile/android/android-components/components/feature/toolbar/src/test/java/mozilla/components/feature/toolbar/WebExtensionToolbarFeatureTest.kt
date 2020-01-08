/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.graphics.Bitmap
import android.graphics.Color
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.BrowserAction
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

typealias WebExtensionBrowserAction = BrowserAction

class WebExtensionToolbarFeatureTest {
    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Test
    fun `render web extension action from browser state`() {
        val defaultBrowserAction =
            WebExtensionBrowserAction("default_title", false, mock(), "", 0, 0) {}
        val overriddenBrowserAction =
            WebExtensionBrowserAction("overridden_title", false, mock(), "", 0, 0) {}
        val toolbar: Toolbar = mock()
        val extensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", true, defaultBrowserAction)
        )
        val overriddenExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", true, overriddenBrowserAction)
        )
        val store = spy(
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
        )
        val webExtToolbarFeature = getWebExtensionToolbarFeature(toolbar, store)
        testDispatcher.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(webExtToolbarFeature).renderWebExtensionActions(any(), any())

        val delegateCaptor = argumentCaptor<WebExtensionToolbarAction>()

        verify(toolbar).addBrowserAction(delegateCaptor.capture())
        assertEquals("overridden_title", delegateCaptor.value.browserAction.title)
    }

    @Test
    fun `does not render actions from disabled extensions `() {
        val enabledAction = WebExtensionBrowserAction("enabled", false, mock(), "", 0, 0) {}
        val disabledAction = WebExtensionBrowserAction("disabled", false, mock(), "", 0, 0) {}
        val toolbar: Toolbar = mock()
        val extensions = mapOf(
            "enabled" to WebExtensionState("enabled", "url", true, enabledAction),
            "disabled" to WebExtensionState("disabled", "url", false, disabledAction)
        )

        val store = spy(
            BrowserStore(
                BrowserState(
                    extensions = extensions
                )
            )
        )
        val webExtToolbarFeature = getWebExtensionToolbarFeature(toolbar, store)
        testDispatcher.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(webExtToolbarFeature, times(1)).renderWebExtensionActions(any(), any())
        val delegateCaptor = argumentCaptor<WebExtensionToolbarAction>()
        verify(toolbar, times(1)).addBrowserAction(delegateCaptor.capture())
        assertEquals("enabled", delegateCaptor.value.browserAction.title)
    }

    @Test
    fun `browser actions can be overridden per tab`() {
        val webExtToolbarFeature = getWebExtensionToolbarFeature()

        val loadIcon: (suspend (Int) -> Bitmap?)? = { mock() }
        val browserAction = BrowserAction(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE
        ) {}

        val browserActionOverride = BrowserAction(
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
        webExtToolbarFeature.renderWebExtensionActions(browserState, mock())

        assertTrue(webExtToolbarFeature.webExtensionBrowserActions.size == 1)
        val ext1 = webExtToolbarFeature.webExtensionBrowserActions["1"]
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
        webExtToolbarFeature.renderWebExtensionActions(browserState, tabSessionState)

        assertTrue(webExtToolbarFeature.webExtensionBrowserActions.size == 1)
        val updatedExt1 = webExtToolbarFeature.webExtensionBrowserActions["1"]
        assertFalse(updatedExt1?.browserAction?.enabled!!)
        assertEquals("updatedText", updatedExt1.browserAction.badgeText!!)
        assertEquals("updatedTitle", updatedExt1.browserAction.title!!)
        assertEquals(loadIcon, updatedExt1.browserAction.loadIcon!!)
        assertEquals(Color.RED, updatedExt1.browserAction.badgeTextColor!!)
        assertEquals(Color.GREEN, updatedExt1.browserAction.badgeBackgroundColor!!)
    }

    private fun getWebExtensionToolbarFeature(
        toolbar: Toolbar = mock(),
        store: BrowserStore = BrowserStore()
    ): WebExtensionToolbarFeature {
        val webExtToolbarFeature = spy(WebExtensionToolbarFeature(toolbar, store))
        val handler: Handler = mock()
        val looper: Looper = mock()
        val iconThread: HandlerThread = mock()

        doReturn(looper).`when`(iconThread).looper
        doReturn(iconThread).`when`(webExtToolbarFeature).iconThread
        doReturn(handler).`when`(webExtToolbarFeature).iconHandler
        webExtToolbarFeature.start()
        return webExtToolbarFeature
    }
}
