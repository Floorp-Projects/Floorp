/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

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
import org.mozilla.fenix.GleanMetrics.CreditCards
import org.mozilla.fenix.GleanMetrics.RecentBookmarks
import org.mozilla.fenix.GleanMetrics.RecentlyVisitedHomepage
import org.mozilla.fenix.GleanMetrics.SyncedTabs
import org.mozilla.fenix.GleanMetrics.TopSites
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class GleanMetricsServiceTest {
    @get:Rule
    val gleanRule = GleanTestRule(testContext)

    private lateinit var gleanService: GleanMetricsService

    @Before
    fun setup() {
        gleanService = GleanMetricsService(testContext)
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
    fun `Home screen recent bookmarks events are correctly recorded`() {
        assertFalse(RecentBookmarks.shown.testHasValue())
        gleanService.track(Event.RecentBookmarksShown)
        assertTrue(RecentBookmarks.shown.testHasValue())

        assertFalse(RecentBookmarks.bookmarkClicked.testHasValue())
        gleanService.track(Event.BookmarkClicked)
        assertTrue(RecentBookmarks.bookmarkClicked.testHasValue())

        assertFalse(RecentBookmarks.showAllBookmarks.testHasValue())
        gleanService.track(Event.ShowAllBookmarks)
        assertTrue(RecentBookmarks.showAllBookmarks.testHasValue())
    }

    @Test
    fun `Home screen recently visited events are correctly recorded`() {
        assertFalse(RecentlyVisitedHomepage.historyHighlightOpened.testHasValue())
        gleanService.track(Event.HistoryHighlightOpened)
        assertTrue(RecentlyVisitedHomepage.historyHighlightOpened.testHasValue())

        assertFalse(RecentlyVisitedHomepage.searchGroupOpened.testHasValue())
        gleanService.track(Event.HistorySearchGroupOpened)
        assertTrue(RecentlyVisitedHomepage.searchGroupOpened.testHasValue())
    }

    @Test
    fun `credit card events are correctly recorded`() {
        assertFalse(CreditCards.saved.testHasValue())
        gleanService.track(Event.CreditCardSaved)
        assertTrue(CreditCards.saved.testHasValue())

        assertFalse(CreditCards.deleted.testHasValue())
        gleanService.track(Event.CreditCardDeleted)
        assertTrue(CreditCards.deleted.testHasValue())

        assertFalse(CreditCards.modified.testHasValue())
        gleanService.track(Event.CreditCardModified)
        assertTrue(CreditCards.modified.testHasValue())

        assertFalse(CreditCards.formDetected.testHasValue())
        gleanService.track(Event.CreditCardFormDetected)
        assertTrue(CreditCards.formDetected.testHasValue())

        assertFalse(CreditCards.autofilled.testHasValue())
        gleanService.track(Event.CreditCardAutofilled)
        assertTrue(CreditCards.autofilled.testHasValue())

        assertFalse(CreditCards.autofillPromptShown.testHasValue())
        gleanService.track(Event.CreditCardAutofillPromptShown)
        assertTrue(CreditCards.autofillPromptShown.testHasValue())

        assertFalse(CreditCards.autofillPromptExpanded.testHasValue())
        gleanService.track(Event.CreditCardAutofillPromptExpanded)
        assertTrue(CreditCards.autofillPromptExpanded.testHasValue())

        assertFalse(CreditCards.autofillPromptDismissed.testHasValue())
        gleanService.track(Event.CreditCardAutofillPromptDismissed)
        assertTrue(CreditCards.autofillPromptDismissed.testHasValue())

        assertFalse(CreditCards.managementAddTapped.testHasValue())
        gleanService.track(Event.CreditCardManagementAddTapped)
        assertTrue(CreditCards.managementAddTapped.testHasValue())

        assertFalse(CreditCards.managementCardTapped.testHasValue())
        gleanService.track(Event.CreditCardManagementCardTapped)
        assertTrue(CreditCards.managementCardTapped.testHasValue())
    }

    @Test
    fun `GIVEN contile top site events WHEN the event is track THEN verify the event is correctly recorded`() {
        assertFalse(TopSites.contileImpression.testHasValue())

        gleanService.track(
            Event.TopSiteContileImpression(
                position = 1,
                source = Event.TopSiteContileImpression.Source.NEWTAB
            )
        )

        assertTrue(TopSites.contileImpression.testHasValue())

        var event = TopSites.contileImpression.testGetValue()

        assertEquals(1, event.size)
        assertEquals("top_sites", event[0].category)
        assertEquals("contile_impression", event[0].name)
        assertEquals("1", event[0].extra!!["position"])
        assertEquals("newtab", event[0].extra!!["source"])

        assertFalse(TopSites.contileClick.testHasValue())

        gleanService.track(
            Event.TopSiteContileClick(
                position = 2,
                source = Event.TopSiteContileClick.Source.NEWTAB
            )
        )

        assertTrue(TopSites.contileClick.testHasValue())

        event = TopSites.contileClick.testGetValue()

        assertEquals(1, event.size)
        assertEquals("top_sites", event[0].category)
        assertEquals("contile_click", event[0].name)
        assertEquals("2", event[0].extra!!["position"])
        assertEquals("newtab", event[0].extra!!["source"])
    }
}
