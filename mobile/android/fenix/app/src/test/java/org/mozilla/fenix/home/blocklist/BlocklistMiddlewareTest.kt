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
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.utils.Settings

class BlocklistMiddlewareTest {
    private val mockSettings: Settings = mockk()
    private val blocklistHandler = BlocklistHandler(mockSettings)

    @Test
    fun `GIVEN empty blocklist WHEN action intercepted THEN unchanged by middleware`() {
        val updatedBookmark = RecentBookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf()
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory
            )
        ).joinBlocking()

        assertEquals(updatedBookmark, store.state.recentBookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with no matching elements THEN unchanged by middleware`() {
        val updatedBookmark = RecentBookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://www.github.org/".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory
            )
        ).joinBlocking()

        assertEquals(updatedBookmark, store.state.recentBookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist with specific pages WHEN action intercepted with matching host THEN unchanged by middleware`() {
        val updatedBookmark = RecentBookmark(url = "https://github.com/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://github.com/mozilla-mobile/fenix".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory
            )
        ).joinBlocking()

        assertEquals(updatedBookmark, store.state.recentBookmarks[0])
    }

    @Test
    fun `GIVEN non-empty blocklist WHEN action intercepted with matching elements THEN filtered by middleware`() {
        val updatedBookmark = RecentBookmark(url = "https://www.mozilla.org/")

        every { mockSettings.homescreenBlocklist } returns setOf("https://www.mozilla.org/".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory
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
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = updatedRecentTabs,
                recentBookmarks = updatedBookmarks,
                recentHistory = store.state.recentHistory
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
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = updatedRecentTabs,
                recentBookmarks = updatedBookmarks,
                recentHistory = store.state.recentHistory
            )
        ).joinBlocking()

        assertEquals(unblockedBookmark, store.state.recentBookmarks[0])
        assertEquals(unblockedRecentTab, store.state.recentTabs[0])
    }

    @Test
    fun `WHEN remove action intercepted THEN hashed url added to blocklist and Change action dispatched`() {
        val captureMiddleware = CaptureActionsMiddleware<HomeFragmentState, HomeFragmentAction>()
        val removedUrl = "https://www.mozilla.org/"
        val removedBookmark = RecentBookmark(url = removedUrl)

        val updateSlot = slot<Set<String>>()
        every { mockSettings.homescreenBlocklist } returns setOf() andThen setOf(removedUrl.stripAndHash())
        every { mockSettings.homescreenBlocklist = capture(updateSlot) } returns Unit
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = HomeFragmentStore(
            HomeFragmentState(recentBookmarks = listOf(removedBookmark)),
            middlewares = listOf(middleware, captureMiddleware)
        )

        store.dispatch(
            HomeFragmentAction.RemoveRecentBookmark(removedBookmark)
        ).joinBlocking()

        val capturedAction = captureMiddleware.findFirstAction(HomeFragmentAction.Change::class)
        assertEquals(emptyList<RecentBookmark>(), capturedAction.recentBookmarks)
        assertEquals(setOf(removedUrl.stripAndHash()), updateSlot.captured)
    }

    @Test
    fun `WHEN urls are compared to blocklist THEN protocols are stripped`() {
        val host = "www.mozilla.org/"
        val updatedBookmark = RecentBookmark(url = "http://$host")

        every { mockSettings.homescreenBlocklist } returns setOf("https://$host".stripAndHash())
        val middleware = BlocklistMiddleware(blocklistHandler)
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory
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
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory
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
        val store = HomeFragmentStore(
            HomeFragmentState(),
            middlewares = listOf(middleware)
        )

        store.dispatch(
            HomeFragmentAction.Change(
                topSites = store.state.topSites,
                mode = store.state.mode,
                collections = store.state.collections,
                tip = store.state.tip,
                showCollectionPlaceholder = store.state.showCollectionPlaceholder,
                recentTabs = store.state.recentTabs,
                recentBookmarks = listOf(updatedBookmark),
                recentHistory = store.state.recentHistory
            )
        ).joinBlocking()

        assertTrue(store.state.recentBookmarks.isEmpty())
    }
}
