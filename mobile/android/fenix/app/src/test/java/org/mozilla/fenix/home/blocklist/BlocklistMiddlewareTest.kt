/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.blocklist

import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.bookmarks.Bookmark
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabState
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class BlocklistMiddlewareTest {
    private val mockSettings: Settings = mockk()
    private val blocklistHandler = BlocklistHandler(mockSettings)

    @Test
    fun `GIVEN empty blocklist WHEN action intercepted THEN unchanged by middleware`() {
        val updatedBookmark = Bookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf()
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = appStore.state.recentTabs,
                bookmarks = listOf(updatedBookmark),
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertEquals(updatedBookmark, appStore.state.bookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with no matching elements THEN unchanged by middleware`() {
        val updatedBookmark = Bookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://www.github.org/".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = appStore.state.recentTabs,
                bookmarks = listOf(updatedBookmark),
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertEquals(updatedBookmark, appStore.state.bookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist with specific pages WHEN action intercepted with matching host THEN unchanged by middleware`() {
        val updatedBookmark = Bookmark(url = "https://github.com/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://github.com/mozilla-mobile/fenix".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = appStore.state.recentTabs,
                bookmarks = listOf(updatedBookmark),
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertEquals(updatedBookmark, appStore.state.bookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with matching elements THEN filtered by middleware`() {
        val updatedBookmark = Bookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://www.mozilla.org/".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = appStore.state.recentTabs,
                bookmarks = listOf(updatedBookmark),
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertTrue(appStore.state.bookmarks.isEmpty())
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with matching elements THEN all relevant sections filtered by middleware`() {
        val blockedUrl = "https://www.mozilla.org/"
        val updatedBookmarks = listOf(Bookmark(url = blockedUrl))
        val updatedRecentTabs = listOf(RecentTab.Tab(createTab(url = blockedUrl)))

        every { mockSettings.homescreenBlocklist } returns setOf(blockedUrl.stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = updatedRecentTabs,
                bookmarks = updatedBookmarks,
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertTrue(appStore.state.bookmarks.isEmpty())
        assertTrue(appStore.state.recentTabs.isEmpty())
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with matching elements THEN only matching urls removed`() {
        val blockedUrl = "https://www.mozilla.org/"
        val unblockedUrl = "https://www.github.org/"
        val unblockedBookmark = Bookmark(unblockedUrl)
        val updatedBookmarks = listOf(
            Bookmark(url = blockedUrl),
            unblockedBookmark,
        )
        val unblockedRecentTab = RecentTab.Tab(createTab(url = unblockedUrl))
        val updatedRecentTabs =
            listOf(RecentTab.Tab(createTab(url = blockedUrl)), unblockedRecentTab)

        every { mockSettings.homescreenBlocklist } returns setOf(blockedUrl.stripAndHash())
        every { mockSettings.frecencyFilterQuery } returns ""
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = updatedRecentTabs,
                bookmarks = updatedBookmarks,
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertEquals(unblockedBookmark, appStore.state.bookmarks[0])
        assertEquals(unblockedRecentTab, appStore.state.recentTabs[0])
    }

    @Test
    fun `WHEN remove action intercepted THEN hashed url added to blocklist and Change action dispatched`() {
        val captureMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
        val removedUrl = "https://www.mozilla.org/"
        val removedBookmark = Bookmark(url = removedUrl)

        val updateSlot = slot<Set<String>>()
        every { mockSettings.homescreenBlocklist } returns setOf() andThen setOf(removedUrl.stripAndHash())
        every { mockSettings.homescreenBlocklist = capture(updateSlot) } returns Unit
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(bookmarks = listOf(removedBookmark)),
            middlewares = listOf(middleware, captureMiddleware),
        )

        appStore.dispatch(
            AppAction.RemoveBookmark(removedBookmark),
        ).joinBlocking()

        val capturedAction = captureMiddleware.findFirstAction(AppAction.Change::class)
        assertEquals(emptyList<Bookmark>(), capturedAction.bookmarks)
        assertEquals(setOf(removedUrl.stripAndHash()), updateSlot.captured)
    }

    @Test
    fun `WHEN urls are compared to blocklist THEN protocols are stripped`() {
        val host = "www.mozilla.org/"
        val updatedBookmark = Bookmark(url = "http://$host")

        every { mockSettings.homescreenBlocklist } returns setOf("https://$host".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = appStore.state.recentTabs,
                bookmarks = listOf(updatedBookmark),
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertTrue(appStore.state.bookmarks.isEmpty())
    }

    @Test
    fun `WHEN urls are compared to blocklist THEN common subdomains are stripped`() {
        val host = "mozilla.org/"
        val updatedBookmark = Bookmark(url = host)

        every { mockSettings.homescreenBlocklist } returns setOf(host.stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = appStore.state.recentTabs,
                bookmarks = listOf(updatedBookmark),
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertTrue(appStore.state.bookmarks.isEmpty())
    }

    @Test
    fun `WHEN urls are compared to blocklist THEN trailing slashes are stripped`() {
        val host = "www.mozilla.org"
        val updatedBookmark = Bookmark(url = "http://$host/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://$host".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.Change(
                topSites = appStore.state.topSites,
                mode = appStore.state.mode,
                collections = appStore.state.collections,
                showCollectionPlaceholder = appStore.state.showCollectionPlaceholder,
                recentTabs = appStore.state.recentTabs,
                bookmarks = listOf(updatedBookmark),
                recentHistory = appStore.state.recentHistory,
                recentSyncedTabState = appStore.state.recentSyncedTabState,
            ),
        ).joinBlocking()

        assertTrue(appStore.state.bookmarks.isEmpty())
    }

    @Test
    fun `WHEN new recently synced tabs are submitted THEN urls matching the blocklist should be removed`() {
        val blockedHost = "https://www.mozilla.org"
        val blockedTab = RecentSyncedTab(
            deviceDisplayName = "",
            deviceType = mock(),
            title = "",
            url = "https://www.mozilla.org",
            previewImageUrl = null,
        )
        val allowedTab = RecentSyncedTab(
            deviceDisplayName = "",
            deviceType = mock(),
            title = "",
            url = "https://github.com",
            previewImageUrl = null,
        )

        every { mockSettings.homescreenBlocklist } returns setOf(blockedHost.stripAndHash())
        every { mockSettings.frecencyFilterQuery } returns ""
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.RecentSyncedTabStateChange(
                RecentSyncedTabState.Success(
                    listOf(
                        blockedTab,
                        allowedTab,
                    ),
                ),
            ),
        ).joinBlocking()

        assertEquals(
            allowedTab,
            (appStore.state.recentSyncedTabState as RecentSyncedTabState.Success).tabs.single(),
        )
    }

    @Test
    fun `WHEN the recent synced tab state is changed to None or Loading THEN the middleware does not change the state`() {
        val blockedHost = "https://www.mozilla.org"
        every { mockSettings.homescreenBlocklist } returns setOf(blockedHost.stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.RecentSyncedTabStateChange(
                RecentSyncedTabState.None,
            ),
        ).joinBlocking()

        assertEquals(RecentSyncedTabState.None, appStore.state.recentSyncedTabState)
    }

    @Test
    fun `WHEN all recently synced submitted tabs are blocked THEN the recent synced tab state should be set to None`() {
        val blockedHost = "https://www.mozilla.org"
        val blockedTab = RecentSyncedTab(
            deviceDisplayName = "",
            deviceType = mock(),
            title = "",
            url = "https://www.mozilla.org",
            previewImageUrl = null,
        )

        every { mockSettings.homescreenBlocklist } returns setOf(blockedHost.stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val appStore = AppStore(
            AppState(),
            middlewares = listOf(middleware),
        )

        appStore.dispatch(
            AppAction.RecentSyncedTabStateChange(
                RecentSyncedTabState.Success(
                    listOf(blockedTab),
                ),
            ),
        ).joinBlocking()

        assertEquals(
            RecentSyncedTabState.None,
            appStore.state.recentSyncedTabState,
        )
    }

    @Test
    fun `WHEN the most recent used synced tab is blocked THEN the following recent synced tabs remain ordered`() {
        val tabUrls = listOf("link1", "link2", "link3")
        val currentTabs = listOf(
            RecentSyncedTab(
                deviceDisplayName = "device1",
                deviceType = mock(),
                title = "",
                url = tabUrls[0],
                previewImageUrl = null,
            ),
            RecentSyncedTab(
                deviceDisplayName = "",
                deviceType = mock(),
                title = "",
                url = tabUrls[1],
                previewImageUrl = null,
            ),
            RecentSyncedTab(
                deviceDisplayName = "",
                deviceType = mock(),
                title = "",
                url = tabUrls[2],
                previewImageUrl = null,
            ),
        )
        val appStore = AppStore(
            AppState(recentSyncedTabState = RecentSyncedTabState.Success(currentTabs)),
            middlewares = listOf(BlocklistMiddleware(blocklistHandler)),
        )
        val updateSlot = slot<Set<String>>()
        every { mockSettings.homescreenBlocklist = capture(updateSlot) } returns Unit
        every { mockSettings.homescreenBlocklist } returns setOf(tabUrls[0].stripAndHash())
        every { mockSettings.frecencyFilterQuery } returns ""

        appStore.dispatch(
            AppAction.RemoveRecentSyncedTab(
                currentTabs.first(),
            ),
        ).joinBlocking()

        assertEquals(
            2,
            (appStore.state.recentSyncedTabState as RecentSyncedTabState.Success).tabs.size,
        )
        assertEquals(setOf(tabUrls[0].stripAndHash()), updateSlot.captured)
        assertEquals(
            currentTabs[1],
            (appStore.state.recentSyncedTabState as RecentSyncedTabState.Success).tabs.firstOrNull(),
        )
    }
}
