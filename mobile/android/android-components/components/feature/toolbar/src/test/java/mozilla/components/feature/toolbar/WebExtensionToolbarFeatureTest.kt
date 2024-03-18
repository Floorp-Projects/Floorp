/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.graphics.Bitmap
import android.graphics.Color
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.concept.engine.webextension.WebExtensionPageAction
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class WebExtensionToolbarFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    @Test
    fun `render web extension actions from browser state`() {
        val defaultPageAction =
            WebExtensionPageAction("default_page_action_title", true, mock(), "", 0, 0) {}
        val overriddenPageAction =
            WebExtensionPageAction("overridden_page_action_title", true, mock(), "", 0, 0) {}
        val defaultBrowserAction =
            WebExtensionBrowserAction("default_browser_action_title", true, mock(), "", 0, 0) {}
        val overriddenBrowserAction =
            WebExtensionBrowserAction("overridden_browser_action_title", true, mock(), "", 0, 0) {}
        val toolbar: Toolbar = mock()
        val extensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", "name", true, browserAction = defaultBrowserAction, pageAction = defaultPageAction),
        )
        val overriddenExtensions: Map<String, WebExtensionState> = mapOf(
            "id" to WebExtensionState("id", "url", "name", true, browserAction = overriddenBrowserAction, pageAction = overriddenPageAction),
        )
        val store = spy(
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
            ),
        )
        val webExtToolbarFeature = getWebExtensionToolbarFeature(toolbar, store)
        dispatcher.scheduler.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(webExtToolbarFeature).renderWebExtensionActions(any(), any())

        val browserActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        verify(toolbar).addBrowserAction(browserActionCaptor.capture())
        assertEquals("overridden_browser_action_title", browserActionCaptor.value.action.title)

        val pageActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        verify(toolbar).addPageAction(pageActionCaptor.capture())
        assertEquals("overridden_page_action_title", pageActionCaptor.value.action.title)
    }

    @Test
    fun `does not render actions from disabled extensions`() {
        val enablePageAction =
            WebExtensionPageAction("enable_page_action", true, mock(), "", 0, 0) {}
        val disablePageAction =
            WebExtensionPageAction("disable_page_action", true, mock(), "", 0, 0) {}
        val enabledAction = WebExtensionBrowserAction("enable_browser_action", true, mock(), "", 0, 0) {}
        val disabledAction = WebExtensionBrowserAction("disable_browser_action", true, mock(), "", 0, 0) {}
        val toolbar: Toolbar = mock()
        val extensions = mapOf(
            "enabled" to WebExtensionState(
                "enabled",
                "url",
                "name",
                true,
                browserAction = enabledAction,
                pageAction = enablePageAction,
            ),
            "disabled" to WebExtensionState(
                "disabled",
                "url",
                "name",
                false,
                browserAction = disabledAction,
                pageAction = disablePageAction,
            ),
        )

        val store = spy(
            BrowserStore(
                BrowserState(
                    extensions = extensions,
                ),
            ),
        )
        val webExtToolbarFeature = getWebExtensionToolbarFeature(toolbar, store)
        dispatcher.scheduler.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(webExtToolbarFeature, times(1)).renderWebExtensionActions(any(), any())
        val browserActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        val pageActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        verify(toolbar, times(1)).addBrowserAction(browserActionCaptor.capture())

        verify(toolbar, times(1)).addPageAction(pageActionCaptor.capture())
        assertEquals("enable_browser_action", browserActionCaptor.value.action.title)
        assertEquals("enable_page_action", pageActionCaptor.value.action.title)
    }

    @Test
    fun `actions can be overridden per tab`() {
        val webExtToolbarFeature = getWebExtensionToolbarFeature()

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
            enabled = true,
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

        // Verify rendering global default browser action
        val browserExtensions = HashMap<String, WebExtensionState>()
        browserExtensions["1"] = WebExtensionState(id = "1", browserAction = browserAction, pageAction = pageAction)

        val browserState = BrowserState(extensions = browserExtensions)
        webExtToolbarFeature.renderWebExtensionActions(browserState, mock())

        // verifying global browser action
        assertEquals(1, webExtToolbarFeature.webExtensionBrowserActions.size)
        var ext1 = webExtToolbarFeature.webExtensionBrowserActions["1"]
        assertTrue(ext1?.action?.enabled!!)
        assertEquals("badgeText", ext1.action.badgeText!!)
        assertEquals("title", ext1.action.title!!)
        assertEquals(loadIcon, ext1.action.loadIcon!!)
        assertEquals(Color.WHITE, ext1.action.badgeTextColor!!)
        assertEquals(Color.BLUE, ext1.action.badgeBackgroundColor!!)

        // verifying global page action
        assertEquals(1, webExtToolbarFeature.webExtensionPageActions.size)
        ext1 = webExtToolbarFeature.webExtensionPageActions["1"]!!
        assertTrue(ext1.action.enabled!!)
        assertEquals("badgeText", ext1.action.badgeText!!)
        assertEquals("title", ext1.action.title!!)
        assertEquals(loadIcon, ext1.action.loadIcon!!)
        assertEquals(Color.WHITE, ext1.action.badgeTextColor!!)
        assertEquals(Color.BLUE, ext1.action.badgeBackgroundColor!!)

        // Verify rendering session-specific actions override
        val tabExtensions = HashMap<String, WebExtensionState>()
        tabExtensions["1"] = WebExtensionState(id = "1", browserAction = browserActionOverride, pageAction = pageActionOverride)

        val tabSessionState = TabSessionState(
            content = mock(),
            extensionState = tabExtensions,
        )
        webExtToolbarFeature.renderWebExtensionActions(browserState, tabSessionState)

        // verifying session-specific browser action
        assertEquals(1, webExtToolbarFeature.webExtensionBrowserActions.size)
        var updatedExt1 = webExtToolbarFeature.webExtensionBrowserActions["1"]
        assertFalse(updatedExt1?.action?.enabled!!)
        assertEquals("updatedText", updatedExt1.action.badgeText!!)
        assertEquals("updatedTitle", updatedExt1.action.title!!)
        assertEquals(loadIcon, updatedExt1.action.loadIcon!!)
        assertEquals(Color.RED, updatedExt1.action.badgeTextColor!!)
        assertEquals(Color.GREEN, updatedExt1.action.badgeBackgroundColor!!)

        // verifying session-specific page action
        assertEquals(1, webExtToolbarFeature.webExtensionPageActions.size)
        updatedExt1 = webExtToolbarFeature.webExtensionPageActions["1"]!!
        assertTrue(updatedExt1.action.enabled!!)
        assertEquals("updatedText", updatedExt1.action.badgeText!!)
        assertEquals("updatedTitle", updatedExt1.action.title!!)
        assertEquals(loadIcon, updatedExt1.action.loadIcon!!)
        assertEquals(Color.RED, updatedExt1.action.badgeTextColor!!)
        assertEquals(Color.GREEN, updatedExt1.action.badgeBackgroundColor!!)
    }

    @Test
    fun `stale actions (from uninstalled or disabled extensions) are removed when feature is restarted`() {
        val browserExtensions = HashMap<String, WebExtensionState>()
        val loadIcon: (suspend (Int) -> Bitmap?)? = { mock() }
        val browserAction = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val pageAction = Action(
            title = "title",
            loadIcon = loadIcon,
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        browserExtensions["1"] =
            WebExtensionState(id = "1", browserAction = browserAction, pageAction = pageAction)

        browserExtensions["2"] =
            WebExtensionState(id = "2", browserAction = browserAction, pageAction = pageAction)

        val browserState = BrowserState(extensions = browserExtensions)
        val store = BrowserStore(browserState)
        val toolbar: Toolbar = mock()
        val webExtToolbarFeature = getWebExtensionToolbarFeature(toolbar, store)

        webExtToolbarFeature.renderWebExtensionActions(browserState, mock())
        assertEquals(2, webExtToolbarFeature.webExtensionBrowserActions.size)
        assertEquals(2, webExtToolbarFeature.webExtensionPageActions.size)

        store.dispatch(WebExtensionAction.UninstallWebExtensionAction("1")).joinBlocking()
        store.dispatch(WebExtensionAction.UpdateWebExtensionEnabledAction("2", false)).joinBlocking()

        webExtToolbarFeature.start()
        assertEquals(0, webExtToolbarFeature.webExtensionBrowserActions.size)
        assertEquals(0, webExtToolbarFeature.webExtensionPageActions.size)

        val browserActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        val pageActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        verify(toolbar, times(2)).removeBrowserAction(browserActionCaptor.capture())
        verify(toolbar, times(2)).removePageAction(pageActionCaptor.capture())
        assertEquals(browserAction, browserActionCaptor.value.action)
        assertEquals(pageAction, pageActionCaptor.value.action)
    }

    @Test
    fun `actions can are sorted per extension name`() {
        val toolbar: Toolbar = mock()
        val webExtToolbarFeature = getWebExtensionToolbarFeature(toolbar)

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
        browserExtensions["1"] = WebExtensionState(id = "1", name = "extensionA", browserAction = actionExt1)
        browserExtensions["2"] = WebExtensionState(id = "2", name = "extensionB", pageAction = actionExt2)

        val browserState = BrowserState(extensions = browserExtensions)
        webExtToolbarFeature.renderWebExtensionActions(browserState, mock())

        val inOrder = inOrder(toolbar)
        val browserActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        inOrder.verify(toolbar).addBrowserAction(browserActionCaptor.capture())
        assertEquals(actionExt1, browserActionCaptor.value.action)

        val pageActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        inOrder.verify(toolbar).addPageAction(pageActionCaptor.capture())
        assertEquals(actionExt2, pageActionCaptor.value.action)
    }

    @Test
    fun `renderWebExtensionActions depends on allowedInPrivateBrowsing and whether the current tab is private`() {
        val toolbar: Toolbar = mock()
        val webExtToolbarFeature = getWebExtensionToolbarFeature(toolbar)
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
        val browserActionCaptor = argumentCaptor<WebExtensionToolbarAction>()

        val browserExtensions = HashMap<String, WebExtensionState>()
        browserExtensions["1"] =
            WebExtensionState(id = "1", name = "extensionA", browserAction = actionExt1)
        val browserState = BrowserState(extensions = browserExtensions)
        webExtToolbarFeature.renderWebExtensionActions(browserState, tabSessionState)
        verify(toolbar, never()).addBrowserAction(browserActionCaptor.capture())

        val browserExtensionsAllowedInPrivateBrowsing = HashMap<String, WebExtensionState>()
        browserExtensionsAllowedInPrivateBrowsing["1"] =
            WebExtensionState(id = "1", allowedInPrivateBrowsing = true, name = "extensionA", browserAction = actionExt1)
        val browserStateAllowedInPrivateBrowsing = BrowserState(extensions = browserExtensionsAllowedInPrivateBrowsing)
        webExtToolbarFeature.renderWebExtensionActions(browserStateAllowedInPrivateBrowsing, tabSessionState)
        verify(toolbar, times(1)).addBrowserAction(browserActionCaptor.capture())
        assertEquals(actionExt1, browserActionCaptor.value.action)
    }

    @Test
    fun `disabled page actions are not rendered`() {
        val enablePageAction =
            WebExtensionPageAction("enable_page_action", true, mock(), "", 0, 0) {}
        val disablePageAction =
            WebExtensionPageAction("disable_page_action", false, mock(), "", 0, 0) {}
        val toolbar: Toolbar = mock()
        val extensions = mapOf(
            "ext1" to WebExtensionState(
                "ext1",
                "url",
                "name",
                true,
                pageAction = enablePageAction,
            ),
            "ext2" to WebExtensionState(
                "ext2",
                "url",
                "name",
                true,
                pageAction = disablePageAction,
            ),
        )

        val store = spy(
            BrowserStore(
                BrowserState(
                    extensions = extensions,
                ),
            ),
        )
        val webExtToolbarFeature = getWebExtensionToolbarFeature(toolbar, store)
        dispatcher.scheduler.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(webExtToolbarFeature).renderWebExtensionActions(any(), any())

        val pageActionCaptor = argumentCaptor<WebExtensionToolbarAction>()
        verify(toolbar, times(1)).addPageAction(pageActionCaptor.capture())
        assertEquals("enable_page_action", pageActionCaptor.value.action.title)
    }

    private fun getWebExtensionToolbarFeature(
        toolbar: Toolbar = mock(),
        store: BrowserStore = BrowserStore(),
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
