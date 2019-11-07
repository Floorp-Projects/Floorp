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
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.TrackingProtectionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.BrowserAction
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.internal.URLRenderer
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

typealias WebExtensionBrowserAction = mozilla.components.concept.engine.webextension.BrowserAction

class ToolbarPresenterTest {
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
    fun `start with no custom tab id registers on store and renders selected tab`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "tab1")),
                customTabs = listOf(createCustomTab("https://www.example.org", id = "ct")),
                selectedTabId = "tab1"))
        )
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))

        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(store).observeManually(any())

        verify(toolbarPresenter).render(any())

        verify(toolbarPresenter.renderer).post("https://www.mozilla.org")
        verify(toolbar).setSearchTerms("")
        verify(toolbar).displayProgress(0)
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.INSECURE
    }

    @Test
    fun `start with custom tab id registers on store and renders custom tab`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "tab1")),
                customTabs = listOf(createCustomTab("https://www.example.org", id = "ct")),
                selectedTabId = "tab1"))
        )
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store, customTabId = "ct"))
        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(toolbarPresenter).render(any())

        verify(toolbarPresenter.renderer).post("https://www.example.org")
        verify(toolbar).setSearchTerms("")
        verify(toolbar).displayProgress(0)
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.INSECURE
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
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))

        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(store).observeManually(any())
        verify(toolbarPresenter).render(any())

        val delegateCaptor = argumentCaptor<WebExtensionToolbarAction>()

        verify(toolbarPresenter.renderer).post("https://www.example.org")
        verify(toolbar).addBrowserAction(delegateCaptor.capture())
        assertEquals("overridden_title", delegateCaptor.value.browserAction.title)
    }

    @Test
    fun `SecurityInfoState change updates toolbar`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "tab1")),
                customTabs = listOf(createCustomTab("https://www.example.org", id = "ct")),
                selectedTabId = "tab1"))
        )

        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))
        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(toolbar, never()).siteSecure = Toolbar.SiteSecurity.SECURE

        store.dispatch(ContentAction.UpdateSecurityInfoAction("tab1", SecurityInfoState(
            secure = true,
            host = "mozilla.org",
            issuer = "Mozilla"
        ))).joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbar).siteSecure = Toolbar.SiteSecurity.SECURE
    }

    @Test
    fun `Toolbar gets cleared when all tabs are removed`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(TabSessionState(
                    id = "tab1",
                    content = ContentState(
                        url = "https://www.mozilla.org",
                        securityInfo = SecurityInfoState(true, "mozilla.org", "Mozilla"),
                        searchTerms = "Hello World",
                        progress = 60
                    )
                )),
                selectedTabId = "tab1"))
        )

        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))
        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(toolbarPresenter.renderer).start()
        verify(toolbarPresenter.renderer).post("https://www.mozilla.org")
        verify(toolbar).setSearchTerms("Hello World")
        verify(toolbar).displayProgress(60)
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.SECURE
        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.OFF_GLOBALLY
        verifyNoMoreInteractions(toolbarPresenter.renderer)
        verifyNoMoreInteractions(toolbar)

        store.dispatch(TabListAction.RemoveTabAction("tab1")).joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbarPresenter.renderer).post("")
        verify(toolbar).setSearchTerms("")
        verify(toolbar).displayProgress(0)
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.INSECURE
    }

    @Test
    fun `Search terms changes updates toolbar`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "tab1")),
                customTabs = listOf(createCustomTab("https://www.example.org", id = "ct")),
                selectedTabId = "tab1"))
        )

        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))
        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(toolbar, never()).setSearchTerms("Hello World")

        store.dispatch(ContentAction.UpdateSearchTermsAction(
            sessionId = "tab1",
            searchTerms = "Hello World")
        ).joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbar).setSearchTerms("Hello World")
    }

    @Test
    fun `Progress changes updates toolbar`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "tab1")),
                customTabs = listOf(createCustomTab("https://www.example.org", id = "ct")),
                selectedTabId = "tab1"))
        )

        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))
        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(toolbar, never()).displayProgress(75)

        store.dispatch(
            ContentAction.UpdateProgressAction("tab1", 75)
        ).joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbar).displayProgress(75)

        verify(toolbar, never()).displayProgress(90)

        store.dispatch(
            ContentAction.UpdateProgressAction("tab1", 90)
        ).joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbar).displayProgress(90)
    }

    @Test
    fun `Toolbar does not get cleared if a background tab gets removed`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(
                    TabSessionState(
                        id = "tab1",
                        content = ContentState(
                            url = "https://www.mozilla.org",
                            securityInfo = SecurityInfoState(true, "mozilla.org", "Mozilla"),
                            searchTerms = "Hello World",
                            progress = 60
                        )
                    ),
                    createTab(id = "tab2", url = "https://www.example.org")),
                selectedTabId = "tab1"))
        )

        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))
        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        store.dispatch(TabListAction.RemoveTabAction("tab2")).joinBlocking()

        verify(toolbarPresenter.renderer).start()
        verify(toolbarPresenter.renderer).post("https://www.mozilla.org")
        verify(toolbar).setSearchTerms("Hello World")
        verify(toolbar).displayProgress(60)
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.SECURE
        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.OFF_GLOBALLY
        verifyNoMoreInteractions(toolbarPresenter.renderer)
        verifyNoMoreInteractions(toolbar)
    }

    @Test
    fun `Toolbar is updated when selected tab changes`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(
                    TabSessionState(
                        id = "tab1",
                        content = ContentState(
                            url = "https://www.mozilla.org",
                            securityInfo = SecurityInfoState(true, "mozilla.org", "Mozilla"),
                            searchTerms = "Hello World",
                            progress = 60
                        )
                    ),
                    TabSessionState(
                        id = "tab2",
                        content = ContentState(
                            url = "https://www.example.org",
                            securityInfo = SecurityInfoState(false, "example.org", "Example"),
                            searchTerms = "Example",
                            progress = 90
                        ),
                        trackingProtection = TrackingProtectionState(enabled = true)
                    )),
                selectedTabId = "tab1"))
        )

        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))
        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(toolbarPresenter.renderer).start()
        verify(toolbarPresenter.renderer).post("https://www.mozilla.org")
        verify(toolbar).setSearchTerms("Hello World")
        verify(toolbar).displayProgress(60)
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.SECURE
        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.OFF_GLOBALLY
        verifyNoMoreInteractions(toolbarPresenter.renderer)
        verifyNoMoreInteractions(toolbar)

        store.dispatch(TabListAction.SelectTabAction("tab2")).joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbarPresenter.renderer).post("https://www.example.org")
        verify(toolbar).setSearchTerms("Example")
        verify(toolbar).displayProgress(90)
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.INSECURE
        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED
        verifyNoMoreInteractions(toolbarPresenter.renderer)
        verifyNoMoreInteractions(toolbar)
    }

    @Test
    fun `displaying different tracking protection states`() {
        val toolbar: Toolbar = mock()
        val store = spy(BrowserStore(
            BrowserState(
                tabs = listOf(
                    TabSessionState(
                        id = "tab",
                        content = ContentState(
                            url = "https://www.mozilla.org",
                            securityInfo = SecurityInfoState(true, "mozilla.org", "Mozilla"),
                            searchTerms = "Hello World",
                            progress = 60
                        )
                    )),
                selectedTabId = "tab")
        ))

        val toolbarPresenter = spy(ToolbarPresenter(toolbar, store))
        toolbarPresenter.renderer = mock()

        toolbarPresenter.start()

        testDispatcher.advanceUntilIdle()

        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.OFF_GLOBALLY

        store.dispatch(TrackingProtectionAction.ToggleAction("tab", true))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED

        store.dispatch(TrackingProtectionAction.TrackerBlockedAction("tab", mock()))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.ON_TRACKERS_BLOCKED

        store.dispatch(TrackingProtectionAction.ToggleExclusionListAction("tab", true))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.OFF_FOR_A_SITE
    }

    @Test
    fun `Stopping presenter stops renderer`() {
        val store = BrowserStore()
        val presenter = ToolbarPresenter(mock(), store)

        val renderer: URLRenderer = mock()
        presenter.renderer = renderer

        presenter.start()

        verify(renderer, never()).stop()

        presenter.stop()

        verify(renderer).stop()
    }

    @Test
    fun `Toolbar displays empty state without tabs`() {
        val store = BrowserStore()
        val toolbar: Toolbar = mock()
        val presenter = ToolbarPresenter(toolbar, store)
        presenter.renderer = mock()

        presenter.start()

        testDispatcher.advanceUntilIdle()

        verify(presenter.renderer).post("")
        verify(toolbar).setSearchTerms("")
        verify(toolbar).displayProgress(0)
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.INSECURE
    }

    @Test
    fun `WebExtensionBrowserAction is replaced when the web extension is already in the toolbar`() {
        val toolbarPresenter = ToolbarPresenter(mock(), mock())

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
        toolbarPresenter.renderWebExtensionActions(browserState, mock())

        assertTrue(toolbarPresenter.webExtensionBrowserActions.size == 2)
        assertTrue(toolbarPresenter.webExtensionBrowserActions["1"]!!.browserAction.enabled)
        assertFalse(toolbarPresenter.webExtensionBrowserActions["2"]!!.browserAction.enabled)

        // Verify tab with existing extension in the toolbar to update its BrowserAction
        val tabExtensions = HashMap<String, WebExtensionState>()
        tabExtensions["3"] = WebExtensionState(id = "1", browserAction = browserActionDisabled)

        val tabSessionState = CustomTabSessionState(
            content = mock(),
            config = mock(),
            extensionState = tabExtensions
        )
        toolbarPresenter.renderWebExtensionActions(browserState, tabSessionState)

        assertTrue(toolbarPresenter.webExtensionBrowserActions.size == 2)
        assertFalse(toolbarPresenter.webExtensionBrowserActions["1"]!!.browserAction.enabled)
        assertFalse(toolbarPresenter.webExtensionBrowserActions["2"]!!.browserAction.enabled)
    }
}
