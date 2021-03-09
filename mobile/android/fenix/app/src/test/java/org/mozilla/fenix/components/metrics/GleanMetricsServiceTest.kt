/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Awesomebar
import org.mozilla.fenix.GleanMetrics.BookmarksManagement
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.History
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.GleanMetrics.SearchDefaultEngine
import org.mozilla.fenix.GleanMetrics.SyncedTabs
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.BrowsersCache

@RunWith(FenixRobolectricTestRunner::class)
class GleanMetricsServiceTest {
    @get:Rule
    val gleanRule = GleanTestRule(testContext)

    private lateinit var gleanService: GleanMetricsService

    @MockK private lateinit var browsersCache: BrowsersCache
    @MockK private lateinit var mozillaProductDetector: MozillaProductDetector

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        val store = BrowserStore()
        gleanService = GleanMetricsService(testContext, lazy { store }, browsersCache, mozillaProductDetector)
    }

    @Test
    fun `setStartupMetrics sets some base metrics`() {
        val expectedAppName = "org.mozilla.fenix"
        every { browsersCache.all(any()).isDefaultBrowser } returns true
        every { mozillaProductDetector.getMozillaBrowserDefault(any()) } returns expectedAppName
        every { mozillaProductDetector.getInstalledMozillaProducts(any()) } returns listOf(expectedAppName)

        gleanService.setStartupMetrics()

        // Verify that browser defaults metrics are set.
        assertEquals(true, Metrics.defaultBrowser.testGetValue())
        assertEquals(expectedAppName, Metrics.defaultMozBrowser.testGetValue())
        assertEquals(listOf(expectedAppName), Metrics.mozillaProducts.testGetValue())

        // Verify that search engine defaults are NOT set. This test does
        // not mock most of the objects telemetry is collected from.
        assertFalse(SearchDefaultEngine.code.testHasValue())
        assertFalse(SearchDefaultEngine.name.testHasValue())
        assertFalse(SearchDefaultEngine.submissionUrl.testHasValue())
    }

    @Test
    fun `the app_opened event is correctly recorded`() {
        // Build the event wrapper used by Fenix.
        val event = Event.OpenedApp(Event.OpenedApp.Source.APP_ICON)

        // Feed the wrapped event in the Glean service.
        gleanService.track(event)

        // Use the testing API to verify that it's correctly recorded.
        assertTrue(Events.appOpened.testHasValue())

        // Get all the recorded events. We only expect 1 to be recorded.
        val events = Events.appOpened.testGetValue()
        assertEquals(1, events.size)

        // Verify that we get the expected content out.
        assertEquals("events", events[0].category)
        assertEquals("app_opened", events[0].name)

        // We only expect 1 extra key.
        assertEquals(1, events[0].extra!!.size)
        assertEquals("APP_ICON", events[0].extra!!["source"])
    }

    @Test
    fun `synced tab event is correctly recorded`() {
        assertFalse(SyncedTabs.syncedTabsSuggestionClicked.testHasValue())
        gleanService.track(Event.SyncedTabSuggestionClicked)
        assertTrue(SyncedTabs.syncedTabsSuggestionClicked.testHasValue())
    }

    @Test
    fun `awesomebar events are correctly recorded`() {
        assertFalse(Awesomebar.bookmarkSuggestionClicked.testHasValue())
        gleanService.track(Event.BookmarkSuggestionClicked)
        assertTrue(Awesomebar.bookmarkSuggestionClicked.testHasValue())

        assertFalse(Awesomebar.clipboardSuggestionClicked.testHasValue())
        gleanService.track(Event.ClipboardSuggestionClicked)
        assertTrue(Awesomebar.clipboardSuggestionClicked.testHasValue())

        assertFalse(Awesomebar.historySuggestionClicked.testHasValue())
        gleanService.track(Event.HistorySuggestionClicked)
        assertTrue(Awesomebar.historySuggestionClicked.testHasValue())

        assertFalse(Awesomebar.searchActionClicked.testHasValue())
        gleanService.track(Event.SearchActionClicked)
        assertTrue(Awesomebar.searchActionClicked.testHasValue())

        assertFalse(Awesomebar.searchSuggestionClicked.testHasValue())
        gleanService.track(Event.SearchSuggestionClicked)
        assertTrue(Awesomebar.searchSuggestionClicked.testHasValue())

        assertFalse(Awesomebar.openedTabSuggestionClicked.testHasValue())
        gleanService.track(Event.OpenedTabSuggestionClicked)
        assertTrue(Awesomebar.openedTabSuggestionClicked.testHasValue())
    }

    @Test
    fun `bookmark events is correctly recorded`() {
        assertFalse(BookmarksManagement.open.testHasValue())
        gleanService.track(Event.OpenedBookmark)
        assertTrue(BookmarksManagement.open.testHasValue())

        assertFalse(BookmarksManagement.openInNewTab.testHasValue())
        gleanService.track(Event.OpenedBookmarkInNewTab)
        assertTrue(BookmarksManagement.openInNewTab.testHasValue())

        assertFalse(BookmarksManagement.openInNewTabs.testHasValue())
        gleanService.track(Event.OpenedBookmarksInNewTabs)
        assertTrue(BookmarksManagement.openInNewTabs.testHasValue())

        assertFalse(BookmarksManagement.openInPrivateTab.testHasValue())
        gleanService.track(Event.OpenedBookmarkInPrivateTab)
        assertTrue(BookmarksManagement.openInPrivateTab.testHasValue())

        assertFalse(BookmarksManagement.openInPrivateTabs.testHasValue())
        gleanService.track(Event.OpenedBookmarksInPrivateTabs)
        assertTrue(BookmarksManagement.openInPrivateTabs.testHasValue())

        assertFalse(BookmarksManagement.edited.testHasValue())
        gleanService.track(Event.EditedBookmark)
        assertTrue(BookmarksManagement.edited.testHasValue())

        assertFalse(BookmarksManagement.moved.testHasValue())
        gleanService.track(Event.MovedBookmark)
        assertTrue(BookmarksManagement.moved.testHasValue())

        assertFalse(BookmarksManagement.removed.testHasValue())
        gleanService.track(Event.RemoveBookmark)
        assertTrue(BookmarksManagement.removed.testHasValue())

        assertFalse(BookmarksManagement.multiRemoved.testHasValue())
        gleanService.track(Event.RemoveBookmarks)
        assertTrue(BookmarksManagement.multiRemoved.testHasValue())

        assertFalse(BookmarksManagement.shared.testHasValue())
        gleanService.track(Event.ShareBookmark)
        assertTrue(BookmarksManagement.shared.testHasValue())

        assertFalse(BookmarksManagement.copied.testHasValue())
        gleanService.track(Event.CopyBookmark)
        assertTrue(BookmarksManagement.copied.testHasValue())

        assertFalse(BookmarksManagement.folderAdd.testHasValue())
        gleanService.track(Event.AddBookmarkFolder)
        assertTrue(BookmarksManagement.folderAdd.testHasValue())

        assertFalse(BookmarksManagement.folderRemove.testHasValue())
        gleanService.track(Event.RemoveBookmarkFolder)
        assertTrue(BookmarksManagement.folderRemove.testHasValue())
    }

    @Test
    fun `History events is correctly recorded`() {
        assertFalse(History.openedItemInNewTab.testHasValue())
        gleanService.track(Event.HistoryOpenedInNewTab)
        assertTrue(History.openedItemInNewTab.testHasValue())

        assertFalse(History.openedItemsInNewTabs.testHasValue())
        gleanService.track(Event.HistoryOpenedInNewTabs)
        assertTrue(History.openedItemsInNewTabs.testHasValue())

        assertFalse(History.openedItemInPrivateTab.testHasValue())
        gleanService.track(Event.HistoryOpenedInPrivateTab)
        assertTrue(History.openedItemInPrivateTab.testHasValue())

        assertFalse(History.openedItemsInPrivateTabs.testHasValue())
        gleanService.track(Event.HistoryOpenedInPrivateTabs)
        assertTrue(History.openedItemsInPrivateTabs.testHasValue())
    }
}
