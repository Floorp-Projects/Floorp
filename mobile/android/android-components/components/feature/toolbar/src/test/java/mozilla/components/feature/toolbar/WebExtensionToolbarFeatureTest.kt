/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.graphics.Color
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.BrowserAction
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

typealias WebExtensionBrowserAction = BrowserAction

class WebExtensionToolbarFeatureTest {
    private val testDispatcher = TestCoroutineDispatcher()

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        Dispatchers.setMain(testDispatcher)
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `render overridden web extension action from browser state`() {
        val defaultBrowserAction =
            WebExtensionBrowserAction("default_title", false, mock(), "", "", 0, 0) {}
        val overriddenBrowserAction =
            WebExtensionBrowserAction("overridden_title", false, mock(), "", "", 0, 0) {}
        val toolbar: Toolbar = mock()
        val extensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", defaultBrowserAction)
        )
        val overriddenExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", overriddenBrowserAction)
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
        val webExtToolbarFeature = spy(WebExtensionToolbarFeature(toolbar, store))

        webExtToolbarFeature.start()

        testDispatcher.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(webExtToolbarFeature).renderWebExtensionActions(any(), any())

        val delegateCaptor = argumentCaptor<WebExtensionToolbarAction>()

        verify(toolbar).addBrowserAction(delegateCaptor.capture())
        assertEquals("overridden_title", delegateCaptor.value.browserAction.title)
    }

    @Test
    fun `WebExtensionBrowserAction is replaced when the web extension is already in the toolbar`() {
        val webExtToolbarFeature = WebExtensionToolbarFeature(mock(), mock())

        val browserAction = BrowserAction(
            title = "title",
            icon = mock(),
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
            uri = "uri"
        ) {}

        val browserActionDisabled = BrowserAction(
            title = "title",
            icon = mock(),
            enabled = false,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
            uri = "uri"
        ) {}

        // Verify browser extension toolbar rendering
        val browserExtensions = HashMap<String, WebExtensionState>()
        browserExtensions["1"] = WebExtensionState(id = "1", browserAction = browserAction)
        browserExtensions["2"] = WebExtensionState(id = "2", browserAction = browserActionDisabled)

        val browserState = BrowserState(extensions = browserExtensions)
        webExtToolbarFeature.renderWebExtensionActions(browserState, mock())

        assertTrue(webExtToolbarFeature.webExtensionBrowserActions.size == 2)
        assertTrue(webExtToolbarFeature.webExtensionBrowserActions["1"]!!.browserAction.enabled)
        assertFalse(webExtToolbarFeature.webExtensionBrowserActions["2"]!!.browserAction.enabled)

        // Verify tab with existing extension in the toolbar to update its BrowserAction
        val tabExtensions = HashMap<String, WebExtensionState>()
        tabExtensions["3"] = WebExtensionState(id = "1", browserAction = browserActionDisabled)

        val tabSessionState = CustomTabSessionState(
                content = mock(),
                config = mock(),
                extensionState = tabExtensions
        )
        webExtToolbarFeature.renderWebExtensionActions(browserState, tabSessionState)

        assertTrue(webExtToolbarFeature.webExtensionBrowserActions.size == 2)
        assertFalse(webExtToolbarFeature.webExtensionBrowserActions["1"]!!.browserAction.enabled)
        assertFalse(webExtToolbarFeature.webExtensionBrowserActions["2"]!!.browserAction.enabled)
    }
}
