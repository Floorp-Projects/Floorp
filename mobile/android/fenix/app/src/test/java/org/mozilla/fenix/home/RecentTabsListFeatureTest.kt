/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.ContentAction.UpdateIconAction
import mozilla.components.browser.state.action.ContentAction.UpdateTitleAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.middleware.LastMediaAccessMiddleware
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.recenttabs.RecentTabsListFeature

class RecentTabsListFeatureTest {

    private lateinit var appStore: AppStore
    private lateinit var middleware: CaptureActionsMiddleware<AppState, AppAction>

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        middleware = CaptureActionsMiddleware()
        appStore = AppStore(middlewares = listOf(middleware))
    }

    @After
    fun teardown() {
        middleware.reset()
    }

    @Test
    fun `GIVEN no selected, last active or in progress media tab WHEN the feature starts THEN dispatch an empty list`() {
        val browserStore = BrowserStore()
        val appStore = AppStore()
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()

        appStore.waitUntilIdle()

        assertEquals(0, appStore.state.recentTabs.size)
    }

    @Test
    fun `GIVEN no selected but last active tab available WHEN the feature starts THEN dispatch the last active tab as a recent tab list`() {
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "1",
        )
        val tabs = listOf(tab)
        val browserStore = BrowserStore(
            BrowserState(tabs = tabs),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()

        appStore.waitUntilIdle()

        assertEquals(1, appStore.state.recentTabs.size)
    }

    @Test
    fun `GIVEN a selected tab WHEN the feature starts THEN dispatch the selected tab as a recent tab list`() {
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "1",
        )
        val tabs = listOf(tab)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = tabs,
                selectedTabId = "1",
            ),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()

        appStore.waitUntilIdle()

        assertEquals(1, appStore.state.recentTabs.size)
    }

    @Ignore("Disabled until we want to enable this feature. See #21670.")
    @Test
    fun `GIVEN a valid inProgressMediaTabId and another selected tab exists WHEN the feature starts THEN dispatch both as as a recent tabs list`() {
        val mediaTab = createTab(
            url = "https://mozilla.com",
            id = "42",
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 123),
        )
        val selectedTab = createTab("https://mozilla.com", id = "43")
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(mediaTab, selectedTab),
                selectedTabId = "43",
            ),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()
        appStore.waitUntilIdle()

        assertEquals(2, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(selectedTab, (appStore.state.recentTabs[0] as RecentTab.Tab).state)
        assertTrue(appStore.state.recentTabs[1] is RecentTab.Tab)
        assertEquals(mediaTab, (appStore.state.recentTabs[1] as RecentTab.Tab).state)
    }

    @Ignore("Disabled until we want to enable this feature. See #21670.")
    @Test
    fun `GIVEN a valid inProgressMediaTabId exists and that is the selected tab WHEN the feature starts THEN dispatch just one tab as the recent tabs list`() {
        val selectedMediaTab = createTab(
            "https://mozilla.com",
            id = "42",
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 123),
        )
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(selectedMediaTab),
                selectedTabId = "42",
            ),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()
        appStore.waitUntilIdle()

        assertEquals(1, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(selectedMediaTab, (appStore.state.recentTabs[0] as RecentTab.Tab).state)
    }

    @Test
    fun `WHEN the browser state has an updated select tab THEN dispatch the new recent tab list`() {
        val tab1 = createTab(
            url = "https://www.mozilla.org",
            id = "1",
        )
        val tab2 = createTab(
            url = "https://www.firefox.com",
            id = "2",
        )
        val tabs = listOf(tab1, tab2)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = tabs,
                selectedTabId = "1",
            ),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()

        appStore.waitUntilIdle()

        assertEquals(1, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(tab1, (appStore.state.recentTabs[0] as RecentTab.Tab).state)

        browserStore.dispatch(TabListAction.SelectTabAction(tab2.id)).joinBlocking()

        appStore.waitUntilIdle()

        assertEquals(1, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(tab2, (appStore.state.recentTabs[0] as RecentTab.Tab).state)
    }

    @Ignore("Disabled until we want to enable this feature. See #21670.")
    @Test
    fun `WHEN the browser state has an in progress media tab THEN dispatch the new recent tab list`() {
        val initialMediaTab = createTab(
            url = "https://mozilla.com",
            id = "1",
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 123),
        )
        val newMediaTab = createTab(
            url = "http://mozilla.org",
            id = "2",
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 100),
        )
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(initialMediaTab, newMediaTab),
                selectedTabId = "1",
            ),
            middleware = listOf(LastMediaAccessMiddleware()),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()
        appStore.waitUntilIdle()
        assertEquals(2, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(initialMediaTab, (appStore.state.recentTabs[0] as RecentTab.Tab).state)

        browserStore.dispatch(
            MediaSessionAction.UpdateMediaPlaybackStateAction("2", MediaSession.PlaybackState.PLAYING),
        ).joinBlocking()
        appStore.waitUntilIdle()
        assertEquals(2, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(initialMediaTab, (appStore.state.recentTabs[0] as RecentTab.Tab).state)
        // UpdateMediaPlaybackStateAction would set the current timestamp as the new value for lastMediaAccess
        val updatedLastMediaAccess =
            (appStore.state.recentTabs[1] as RecentTab.Tab).state.lastMediaAccessState.lastMediaAccess
        assertTrue("expected lastMediaAccess ($updatedLastMediaAccess) > 100", updatedLastMediaAccess > 100)
        assertEquals(
            "http://mozilla.org",
            (appStore.state.recentTabs[1] as RecentTab.Tab).state.lastMediaAccessState.lastMediaUrl,
        )
        // Check that the media tab is updated ignoring just the lastMediaAccess property.
        assertEquals(
            newMediaTab,
            (appStore.state.recentTabs[1] as RecentTab.Tab).state.copy(
                lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 100),
            ),
        )
    }

    @Test
    fun `WHEN the browser state selects a private tab THEN dispatch an empty list`() {
        val selectedNormalTab = createTab(
            url = "https://www.mozilla.org",
            id = "1",
            lastAccess = 0,
        )
        val lastAccessedNormalTab = createTab(
            url = "https://www.mozilla.org",
            id = "2",
            lastAccess = 1,
        )
        val privateTab = createTab(
            url = "https://www.firefox.com",
            id = "3",
            private = true,
        )
        val tabs = listOf(selectedNormalTab, lastAccessedNormalTab, privateTab)
        val browserStore = BrowserStore(
            BrowserState(
                tabs = tabs,
                selectedTabId = "1",
            ),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()

        appStore.waitUntilIdle()

        assertEquals(1, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(selectedNormalTab, (appStore.state.recentTabs[0] as RecentTab.Tab).state)

        browserStore.dispatch(TabListAction.SelectTabAction(privateTab.id)).joinBlocking()

        appStore.waitUntilIdle()

        // If the selected tab is a private tab the feature should show the last accessed normal tab.
        assertEquals(1, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(lastAccessedNormalTab, (appStore.state.recentTabs[0] as RecentTab.Tab).state)
    }

    @Test
    fun `WHEN the selected tabs title or icon update THEN update the home store`() {
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://www.mozilla.org",
                        id = "1",
                    ),
                ),
                selectedTabId = "1",
            ),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()

        appStore.waitUntilIdle()

        middleware.assertLastAction(AppAction.RecentTabsChange::class) {
            val tab = it.recentTabs.first() as RecentTab.Tab
            assertTrue(tab.state.content.title.isEmpty())
            assertNull(tab.state.content.icon)
        }

        browserStore.dispatch(UpdateTitleAction("1", "test")).joinBlocking()

        appStore.waitUntilIdle()

        middleware.assertLastAction(AppAction.RecentTabsChange::class) {
            val tab = it.recentTabs.first() as RecentTab.Tab
            assertEquals("test", tab.state.content.title)
            assertNull(tab.state.content.icon)
        }

        browserStore.dispatch(UpdateIconAction("1", "https://www.mozilla.org", mockk()))
            .joinBlocking()

        appStore.waitUntilIdle()

        middleware.assertLastAction(AppAction.RecentTabsChange::class) {
            val tab = it.recentTabs.first() as RecentTab.Tab
            assertEquals("test", tab.state.content.title)
            assertNotNull(tab.state.content.icon)
        }
    }

    @Test
    fun `GIVEN inProgressMediaTab already set WHEN the media tab is closed THEN remove it from recent tabs`() {
        val initialMediaTab = createTab(url = "https://mozilla.com", id = "1")
        val selectedTab = createTab(url = "https://mozilla.com/firefox", id = "2")
        val browserStore = BrowserStore(
            initialState = BrowserState(listOf(initialMediaTab, selectedTab), selectedTabId = "2"),
            middleware = listOf(LastMediaAccessMiddleware()),
        )
        val feature = RecentTabsListFeature(
            browserStore = browserStore,
            appStore = appStore,
        )

        feature.start()
        browserStore.dispatch(TabListAction.RemoveTabsAction(listOf("1"))).joinBlocking()
        appStore.waitUntilIdle()

        assertEquals(1, appStore.state.recentTabs.size)
        assertTrue(appStore.state.recentTabs[0] is RecentTab.Tab)
        assertEquals(selectedTab, (appStore.state.recentTabs[0] as RecentTab.Tab).state)
    }
}
