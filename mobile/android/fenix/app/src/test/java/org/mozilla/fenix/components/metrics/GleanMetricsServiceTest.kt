/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import io.mockk.MockKAnnotations
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.Awesomebar
import org.mozilla.fenix.GleanMetrics.BookmarksManagement
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.History
import org.mozilla.fenix.GleanMetrics.SyncedTabs
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class GleanMetricsServiceTest {
    @get:Rule
    val gleanRule = GleanTestRule(testContext)

    private lateinit var gleanService: GleanMetricsService

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        gleanService = GleanMetricsService(testContext)
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
    fun `bookmark events are correctly recorded`() {
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
    fun `History events are correctly recorded`() {
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

    @Test
    fun `Addon events are correctly recorded`() {
        assertFalse(Addons.openAddonsInSettings.testHasValue())
        gleanService.track(Event.AddonsOpenInSettings)
        assertTrue(Addons.openAddonsInSettings.testHasValue())

        assertFalse(Addons.openAddonInToolbarMenu.testHasValue())
        gleanService.track(Event.AddonsOpenInToolbarMenu("123"))
        assertTrue(Addons.openAddonInToolbarMenu.testHasValue())
        var events = Addons.openAddonInToolbarMenu.testGetValue()
        assertEquals(1, events.size)
        assertEquals("addons", events[0].category)
        assertEquals("open_addon_in_toolbar_menu", events[0].name)
        assertEquals(1, events[0].extra!!.size)
        assertEquals("123", events[0].extra!!["addon_id"])

        assertFalse(Addons.openAddonSetting.testHasValue())
        gleanService.track(Event.AddonOpenSetting("123"))
        assertTrue(Addons.openAddonSetting.testHasValue())
        events = Addons.openAddonSetting.testGetValue()
        assertEquals(1, events.size)
        assertEquals("addons", events[0].category)
        assertEquals("open_addon_setting", events[0].name)
        assertEquals(1, events[0].extra!!.size)
        assertEquals("123", events[0].extra!!["addon_id"])
    }

    @Test
    fun `TabsTray events are correctly recorded`() {
        assertFalse(TabsTray.opened.testHasValue())
        gleanService.track(Event.TabsTrayOpened)
        assertTrue(TabsTray.opened.testHasValue())

        assertFalse(TabsTray.closed.testHasValue())
        gleanService.track(Event.TabsTrayClosed)
        assertTrue(TabsTray.closed.testHasValue())

        assertFalse(TabsTray.openedExistingTab.testHasValue())
        gleanService.track(Event.OpenedExistingTab)
        assertTrue(TabsTray.openedExistingTab.testHasValue())

        assertFalse(TabsTray.closedExistingTab.testHasValue())
        gleanService.track(Event.ClosedExistingTab)
        assertTrue(TabsTray.closedExistingTab.testHasValue())

        assertFalse(TabsTray.privateModeTapped.testHasValue())
        gleanService.track(Event.TabsTrayPrivateModeTapped)
        assertTrue(TabsTray.privateModeTapped.testHasValue())

        assertFalse(TabsTray.normalModeTapped.testHasValue())
        gleanService.track(Event.TabsTrayNormalModeTapped)
        assertTrue(TabsTray.normalModeTapped.testHasValue())

        assertFalse(TabsTray.syncedModeTapped.testHasValue())
        gleanService.track(Event.TabsTraySyncedModeTapped)
        assertTrue(TabsTray.syncedModeTapped.testHasValue())

        assertFalse(TabsTray.newTabTapped.testHasValue())
        gleanService.track(Event.NewTabTapped)
        assertTrue(TabsTray.newTabTapped.testHasValue())

        assertFalse(TabsTray.newPrivateTabTapped.testHasValue())
        gleanService.track(Event.NewPrivateTabTapped)
        assertTrue(TabsTray.newPrivateTabTapped.testHasValue())

        assertFalse(TabsTray.menuOpened.testHasValue())
        gleanService.track(Event.TabsTrayMenuOpened)
        assertTrue(TabsTray.menuOpened.testHasValue())

        assertFalse(TabsTray.saveToCollection.testHasValue())
        gleanService.track(Event.TabsTraySaveToCollectionPressed)
        assertTrue(TabsTray.saveToCollection.testHasValue())

        assertFalse(TabsTray.shareAllTabs.testHasValue())
        gleanService.track(Event.TabsTrayShareAllTabsPressed)
        assertTrue(TabsTray.shareAllTabs.testHasValue())

        assertFalse(TabsTray.closeAllTabs.testHasValue())
        gleanService.track(Event.TabsTrayCloseAllTabsPressed)
        assertTrue(TabsTray.closeAllTabs.testHasValue())

        assertFalse(TabsTray.inactiveTabsRecentlyClosed.testHasValue())
        gleanService.track(Event.TabsTrayRecentlyClosedPressed)
        assertTrue(TabsTray.inactiveTabsRecentlyClosed.testHasValue())
    }

    @Test
    fun `default browser events are correctly recorded`() {
        assertFalse(Events.defaultBrowserChanged.testHasValue())
        gleanService.track(Event.ChangedToDefaultBrowser)
        assertTrue(Events.defaultBrowserChanged.testHasValue())

        assertFalse(Events.defaultBrowserNotifTapped.testHasValue())
        gleanService.track(Event.DefaultBrowserNotifTapped)
        assertTrue(Events.defaultBrowserNotifTapped.testHasValue())
    }
}
