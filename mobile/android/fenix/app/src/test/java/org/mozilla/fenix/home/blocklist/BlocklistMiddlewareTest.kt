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
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
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
        val updatedBookmark = RecentBookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf()
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState,
            )
        ).joinBlocking()

        assertEquals(updatedBookmark, store.state.recentBookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with no matching elements THEN unchanged by middleware`() {
        val updatedBookmark = RecentBookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://www.github.org/".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState,
            )
        ).joinBlocking()

        assertEquals(updatedBookmark, store.state.recentBookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist with specific pages WHEN action intercepted with matching host THEN unchanged by middleware`() {
        val updatedBookmark = RecentBookmark(url = "https://github.com/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://github.com/mozilla-mobile/fenix".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState,
            )
        ).joinBlocking()

        assertEquals(updatedBookmark, store.state.recentBookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with matching elements THEN filtered by middleware`() {
        val updatedBookmark = RecentBookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://www.mozilla.org/".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState
            )
        ).joinBlocking()

        assertTrue(store.state.recentBookmarks.isEmpty())
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with matching elements THEN all relevant sections filtered by middleware`() {
        val blockedUrl = "https://www.mozilla.org/"
        val updatedBookmarks = listOf(RecentBookmark(url = blockedUrl))
        val updatedRecentTabs = listOf(RecentTab.Tab(createTab(url = blockedUrl)))

        every { mockSettings.homescreenBlocklist } returns setOf(blockedUrl.stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = updatedRecentTabs,
                recentBookmarks = updatedBookmarks,
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState
            )
        ).joinBlocking()

        assertTrue(store.state.recentBookmarks.isEmpty())
        assertTrue(store.state.recentTabs.isEmpty())
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with matching elements THEN only matching urls removed`() {
        val blockedUrl = "https://www.mozilla.org/"
        val unblockedUrl = "https://www.github.org/"
        val unblockedBookmark = RecentBookmark(unblockedUrl)
        val updatedBookmarks = listOf(
            RecentBookmark(url = blockedUrl), unblockedBookmark
        )
        val unblockedRecentTab = RecentTab.Tab(createTab(url = unblockedUrl))
        val updatedRecentTabs =
            listOf(RecentTab.Tab(createTab(url = blockedUrl)), unblockedRecentTab)

        every { mockSettings.homescreenBlocklist } returns setOf(blockedUrl.stripAndHash())
        every { mockSettings.frecencyFilterQuery } returns ""
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = updatedRecentTabs,
                recentBookmarks = updatedBookmarks,
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState
            )
        ).joinBlocking()

        assertEquals(unblockedBookmark, store.state.recentBookmarks[0])
        assertEquals(unblockedRecentTab, store.state.recentTabs[0])
    }

    @Test
    fun `WHEN remove action intercepted THEN hashed url added to blocklist and Change action dispatched`() {
        val captureMiddleware = CaptureActionsMiddleware<AppState, AppAction>()
        val removedUrl = "https://www.mozilla.org/"
        val removedBookmark = RecentBookmark(url = removedUrl)

        val updateSlot = slot<Set<String>>()
        every { mockSettings.homescreenBlocklist } returns setOf() andThen setOf(removedUrl.stripAndHash())
        every { mockSettings.homescreenBlocklist = capture(updateSlot) } returns Unit
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(recentBookmarks = listOf(removedBookmark)),
            middlewares = listOf(middleware, captureMiddleware)
        )

        store.dispatch(
            AppAction.RemoveRecentBookmark(removedBookmark)
        ).joinBlocking()

        val capturedAction = captureMiddleware.findFirstAction(AppAction.Change::class)
        assertEquals(emptyList<RecentBookmark>(), capturedAction.recentBookmarks)
        assertEquals(setOf(removedUrl.stripAndHash()), updateSlot.captured)
    }

    @Test
    fun `WHEN urls are compared to blocklist THEN protocols are stripped`() {
        val host = "www.mozilla.org/"
        val updatedBookmark = RecentBookmark(url = "http://$host")

        every { mockSettings.homescreenBlocklist } returns setOf("https://$host".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState
            )
        ).joinBlocking()

        assertTrue(store.state.recentBookmarks.isEmpty())
    }

    @Test
    fun `WHEN urls are compared to blocklist THEN common subdomains are stripped`() {
        val host = "mozilla.org/"
        val updatedBookmark = RecentBookmark(url = host)

        every { mockSettings.homescreenBlocklist } returns setOf(host.stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState
            )
        ).joinBlocking()

        assertTrue(store.state.recentBookmarks.isEmpty())
    }

    @Test
    fun `WHEN urls are compared to blocklist THEN trailing slashes are stripped`() {
        val host = "www.mozilla.org"
        val updatedBookmark = RecentBookmark(url = "http://$host/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://$host".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory,
                recentSyncedTabState = store.state.recentSyncedTabState
            )
        ).joinBlocking()

        assertTrue(store.state.recentBookmarks.isEmpty())
    }

    @Test
    fun `WHEN new recently synced tabs are submitted THEN urls matching the blocklist should be removed`() {
        val blockedHost = "https://www.mozilla.org"
        val blockedTab = RecentSyncedTab(
            deviceDisplayName = "",
            deviceType = mock(),
            title = "",
            url = "https://www.mozilla.org",
            previewImageUrl = null
        )
        val allowedTab = RecentSyncedTab(
            deviceDisplayName = "",
            deviceType = mock(),
            title = "",
            url = "https://github.com",
            previewImageUrl = null
        )

        every { mockSettings.homescreenBlocklist } returns setOf(blockedHost.stripAndHash())
        every { mockSettings.frecencyFilterQuery } returns ""
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.RecentSyncedTabStateChange(
                RecentSyncedTabState.Success(
                    listOf(
                        blockedTab,
                        allowedTab
                    )
                )
            )
        ).joinBlocking()

        assertEquals(
            allowedTab,
            (store.state.recentSyncedTabState as RecentSyncedTabState.Success).tabs.single()
        )
    }

    @Test
    fun `WHEN the recent synced tab state is changed to None or Loading THEN the middleware does not change the state`() {
        val blockedHost = "https://www.mozilla.org"
        every { mockSettings.homescreenBlocklist } returns setOf(blockedHost.stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.RecentSyncedTabStateChange(
                RecentSyncedTabState.None
            )
        ).joinBlocking()

        assertEquals(RecentSyncedTabState.None, store.state.recentSyncedTabState)
    }

    @Test
    fun `WHEN all recently synced submitted tabs are blocked THEN the recent synced tab state should be set to None`() {
        val blockedHost = "https://www.mozilla.org"
        val blockedTab = RecentSyncedTab(
            deviceDisplayName = "",
            deviceType = mock(),
            title = "",
            url = "https://www.mozilla.org",
            previewImageUrl = null
        )

        every { mockSettings.homescreenBlocklist } returns setOf(blockedHost.stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = AppStore(
            AppState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            AppAction.RecentSyncedTabStateChange(
                RecentSyncedTabState.Success(
                    listOf(blockedTab)
                )
            )
        ).joinBlocking()

        assertEquals(
            RecentSyncedTabState.None,
            store.state.recentSyncedTabState
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
                previewImageUrl = null
            ),
            RecentSyncedTab(
                deviceDisplayName = "",
                deviceType = mock(),
                title = "",
                url = tabUrls[1],
                previewImageUrl = null
            ),
            RecentSyncedTab(
                deviceDisplayName = "",
                deviceType = mock(),
                title = "",
                url = tabUrls[2],
                previewImageUrl = null
            )
        )
        val store = AppStore(
            AppState(recentSyncedTabState = RecentSyncedTabState.Success(currentTabs)),
            middlewares = listOf(BlocklistMiddleware(blocklistHandler))
        )
        val updateSlot = slot<Set<String>>()
        every { mockSettings.homescreenBlocklist = capture(updateSlot) } returns Unit
        every { mockSettings.homescreenBlocklist } returns setOf(tabUrls[0].stripAndHash())
        every { mockSettings.frecencyFilterQuery } returns ""

        store.dispatch(
            AppAction.RemoveRecentSyncedTab(
                currentTabs.first()
            )
        ).joinBlocking()

        assertEquals(
            2, (store.state.recentSyncedTabState as RecentSyncedTabState.Success).tabs.size
        )
        assertEquals(setOf(tabUrls[0].stripAndHash()), updateSlot.captured)
        assertEquals(
            currentTabs[1],
            (store.state.recentSyncedTabState as RecentSyncedTabState.Success).tabs.firstOrNull()
        )
    }
}
