/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.blocklist

import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.sync.DeviceType
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabState
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.recentvisits.RecentlyVisitedItem
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class BlocklistHandlerTest {
    private val mockSettings: Settings = mockk()

    private lateinit var blocklistHandler: BlocklistHandler

    @Before
    fun setup() {
        blocklistHandler = BlocklistHandler(mockSettings)
    }

    @Test
    fun `WHEN url added to blocklist THEN settings updated with hash`() {
        val addedUrl = "url"
        val updateSlot = slot<Set<String>>()
        every { mockSettings.homescreenBlocklist } returns setOf()
        every { mockSettings.homescreenBlocklist = capture(updateSlot) } returns Unit

        blocklistHandler.addUrlToBlocklist(addedUrl)

        assertEquals(setOf(addedUrl.stripAndHash()), updateSlot.captured)
    }

    @Test
    fun `GIVEN bookmark is not in blocklist THEN will not be filtered`() {
        val bookmarks = listOf(RecentBookmark(url = "test"))
        every { mockSettings.homescreenBlocklist } returns setOf()

        val filtered = with(blocklistHandler) {
            bookmarks.filteredByBlocklist()
        }

        assertEquals(bookmarks, filtered)
    }

    @Test
    fun `GIVEN bookmark is in blocklist THEN will be filtered`() {
        val blockedUrl = "test"
        val bookmarks = listOf(RecentBookmark(url = blockedUrl))
        every { mockSettings.homescreenBlocklist } returns setOf(blockedUrl.stripAndHash())

        val filtered = with(blocklistHandler) {
            bookmarks.filteredByBlocklist()
        }

        assertEquals(listOf<String>(), filtered)
    }

    @Test
    fun `GIVEN recent history is not in blocklist THEN will not be filtered`() {
        val recentHistory = listOf(RecentlyVisitedItem.RecentHistoryHighlight(url = "test", title = ""))
        every { mockSettings.homescreenBlocklist } returns setOf()

        val filtered = with(blocklistHandler) {
            recentHistory.filteredByBlocklist()
        }

        assertEquals(recentHistory, filtered)
    }

    @Test
    fun `GIVEN recent history is in blocklist THEN will be filtered`() {
        val blockedUrl = "test"
        val recentHistory = listOf(RecentlyVisitedItem.RecentHistoryHighlight(url = blockedUrl, title = ""))
        every { mockSettings.homescreenBlocklist } returns setOf(blockedUrl.stripAndHash())

        val filtered = with(blocklistHandler) {
            recentHistory.filteredByBlocklist()
        }

        assertEquals(listOf<String>(), filtered)
    }

    @Test
    fun `GIVEN recent tab is not in blocklist THEN will not be filtered`() {
        val mockSessionState: TabSessionState = mockk()
        val mockContent: ContentState = mockk()
        val tabs = listOf(RecentTab.Tab(mockSessionState))
        every { mockSessionState.content } returns mockContent
        every { mockContent.url } returns "test"
        every { mockSettings.homescreenBlocklist } returns setOf()

        val filtered = with(blocklistHandler) {
            tabs.filteredByBlocklist()
        }

        assertEquals(tabs, filtered)
    }

    @Test
    fun `GIVEN recent tab is in blocklist THEN will be filtered`() {
        val blockedUrl = "test"
        val mockSessionState: TabSessionState = mockk()
        val mockContent: ContentState = mockk()
        val tabs = listOf(RecentTab.Tab(mockSessionState))
        every { mockSessionState.content } returns mockContent
        every { mockContent.url } returns blockedUrl
        every { mockSettings.homescreenBlocklist } returns setOf(blockedUrl.stripAndHash())

        val filtered = with(blocklistHandler) {
            tabs.filteredByBlocklist()
        }

        assertEquals(listOf<String>(), filtered)
    }

    @Test
    fun `GIVEN recently synced tab is a sponsored url WHEN the tabs are filtered THEN the sponsored url be filtered`() {
        val blockedUrl = "test.com/?query=value"
        val mockSessionState: TabSessionState = mockk()
        val mockContent: ContentState = mockk()
        val tabs = RecentSyncedTabState.Success(
            listOf(
                RecentSyncedTab(
                    "",
                    DeviceType.DESKTOP,
                    "title",
                    blockedUrl,
                    null,
                ),
            ),
        )
        every { mockSessionState.content } returns mockContent
        every { mockContent.url } returns blockedUrl
        every { mockSettings.frecencyFilterQuery } returns "query=value"

        val filtered = with(blocklistHandler) {
            tabs.filterContile()
        }

        assertEquals(RecentSyncedTabState.None, filtered)
    }

    @Test
    fun `GIVEN recently visited item is a sponsored url WHEN the tabs are filtered THEN the sponsored url be filtered`() {
        val blockedUrl = "test.com/?query=value"
        val mockSessionState: TabSessionState = mockk()
        val mockContent: ContentState = mockk()
        val tabs = listOf(RecentlyVisitedItem.RecentHistoryHighlight("title", blockedUrl))
        every { mockSessionState.content } returns mockContent
        every { mockContent.url } returns blockedUrl
        every { mockSettings.frecencyFilterQuery } returns "query=value"

        val filtered = with(blocklistHandler) {
            tabs.filterContile()
        }

        assertEquals(listOf<String>(), filtered)
    }

    @Test
    fun `GIVEN recent tab is a sponsored url WHEN the tabs are filtered THEN the sponsored url be filtered`() {
        val blockedUrl = "test.com/?query=value"
        val mockSessionState: TabSessionState = mockk()
        val mockContent: ContentState = mockk()
        val tabs = listOf(RecentTab.Tab(mockSessionState))
        every { mockSessionState.content } returns mockContent
        every { mockContent.url } returns blockedUrl
        every { mockSettings.frecencyFilterQuery } returns "query=value"

        val filtered = with(blocklistHandler) {
            tabs.filterContile()
        }

        assertEquals(listOf<String>(), filtered)
    }
}
