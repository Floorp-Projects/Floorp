/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import android.app.Activity
import android.graphics.drawable.VectorDrawable
import android.net.Uri
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.CombinedHistorySuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchActionProvider
import mozilla.components.feature.awesomebar.provider.SearchEngineSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchTermSuggestionsProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.fxsuggest.FxSuggestSuggestionProvider
import mozilla.components.feature.syncedtabs.SyncedTabsStorageSuggestionProvider
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Core.Companion
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.awesomebar.AwesomeBarView.SearchProviderState
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class AwesomeBarViewTest {
    private var activity: HomeActivity = mockk(relaxed = true)
    private var appStore = AppStore()
    private lateinit var awesomeBarView: AwesomeBarView

    @Before
    fun setup() {
        // The following setup is needed to complete the init block of AwesomeBarView
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        mockkStatic("mozilla.components.support.ktx.android.content.ContextKt")
        mockkObject(AwesomeBarView.Companion)
        every { any<Activity>().components.core.engine } returns mockk()
        every { any<Activity>().components.core.icons } returns mockk()
        every { any<Activity>().components.core.store } returns mockk()
        every { any<Activity>().components.core.historyStorage } returns mockk()
        every { any<Activity>().components.core.bookmarksStorage } returns mockk()
        every { any<Activity>().components.core.client } returns mockk()
        every { any<Activity>().components.backgroundServices.syncedTabsStorage } returns mockk()
        every { any<Activity>().components.core.store.state.search } returns mockk(relaxed = true)
        every { any<Activity>().components.appStore } returns appStore
        every { any<Activity>().getColorFromAttr(any()) } returns 0
        every { AwesomeBarView.Companion.getDrawable(any(), any()) } returns mockk<VectorDrawable>(relaxed = true) {
            every { intrinsicWidth } returns 10
            every { intrinsicHeight } returns 10
        }

        awesomeBarView = AwesomeBarView(
            activity = activity,
            interactor = mockk(),
            view = mockk(),
            fromHomeFragment = false,
        )
    }

    @After
    fun tearDown() {
        unmockkStatic("org.mozilla.fenix.ext.ContextKt")
        unmockkStatic("mozilla.components.support.ktx.android.content.ContextKt")
        unmockkObject(AwesomeBarView.Companion)
    }

    @Test
    fun `GIVEN a search from history and history metadata is enabled and sponsored suggestions are enabled WHEN setting the providers THEN set less suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals(
            AwesomeBarView.METADATA_SUGGESTION_LIMIT,
            (historyProvider as CombinedHistorySuggestionProvider).getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN a search from history and history metadata is enabled and sponsored suggestions are disabled WHEN setting the providers THEN set more suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals(
            Companion.METADATA_HISTORY_SUGGESTION_LIMIT,
            (historyProvider as CombinedHistorySuggestionProvider).getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN a search from history and history metadata is disabled and sponsored suggestions are enabled WHEN setting the providers THEN set less suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals(
            AwesomeBarView.METADATA_SUGGESTION_LIMIT,
            (historyProvider as HistoryStorageSuggestionProvider).getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN a search from history and history metadata is disabled and sponsored suggestions are disabled WHEN setting the providers THEN set more suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals(
            Companion.METADATA_HISTORY_SUGGESTION_LIMIT,
            (historyProvider as HistoryStorageSuggestionProvider).getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN a search not from history and history metadata is enabled and sponsored suggestions are enabled WHEN setting the providers THEN set less suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true)),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals(
            AwesomeBarView.METADATA_SUGGESTION_LIMIT,
            (historyProvider as CombinedHistorySuggestionProvider).getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN a search not from history and history metadata is enabled and sponsored suggestions are disabled WHEN setting the providers THEN set less suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true)),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals(
            AwesomeBarView.METADATA_SUGGESTION_LIMIT,
            (historyProvider as CombinedHistorySuggestionProvider).getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN a search not from history and history metadata is disabled and sponsored suggestions are enabled WHEN setting the providers THEN set less suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Bookmarks(mockk(relaxed = true)),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals(
            AwesomeBarView.METADATA_SUGGESTION_LIMIT,
            (historyProvider as CombinedHistorySuggestionProvider).getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN a search not from history and history metadata disabled WHEN setting the providers THEN set less suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Bookmarks(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals(
            AwesomeBarView.METADATA_SUGGESTION_LIMIT,
            (historyProvider as CombinedHistorySuggestionProvider).getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN a search that should show filtered history WHEN history metadata is enabled and sponsored suggestions are enabled THEN return a history metadata provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        val url = Uri.parse("test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllHistorySuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertNotNull((historyProvider as CombinedHistorySuggestionProvider).resultsUriFilter)
        assertEquals(AwesomeBarView.METADATA_SUGGESTION_LIMIT, historyProvider.getMaxNumberOfSuggestions())
    }

    @Test
    fun `GIVEN a search that should show filtered history WHEN history metadata is enabled and sponsored suggestions are disabled THEN return a history metadata provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        val url = Uri.parse("test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllHistorySuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertNotNull((historyProvider as CombinedHistorySuggestionProvider).resultsUriFilter)
        assertEquals(
            AwesomeBarView.METADATA_SUGGESTION_LIMIT,
            historyProvider.getMaxNumberOfSuggestions(),
        )
    }

    @Test
    fun `GIVEN the default engine is selected WHEN history metadata is enabled THEN suggestions are disabled in history and bookmark providers`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showHistorySuggestionsForCurrentEngine = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showSessionSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val combinedHistoryProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider } as CombinedHistorySuggestionProvider
        assertNotNull(combinedHistoryProvider)
        assertFalse(combinedHistoryProvider.showEditSuggestion)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider } as BookmarksStorageSuggestionProvider
        assertNotNull(bookmarkProvider)
        assertFalse(bookmarkProvider.showEditSuggestion)
    }

    @Test
    fun `GIVEN the default engine is selected WHEN history metadata is disabled THEN suggestions are disabled in history and bookmark providers`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showHistorySuggestionsForCurrentEngine = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showSessionSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val defaultHistoryProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider } as HistoryStorageSuggestionProvider
        assertNotNull(defaultHistoryProvider)
        assertFalse(defaultHistoryProvider.showEditSuggestion)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider } as BookmarksStorageSuggestionProvider
        assertNotNull(bookmarkProvider)
        assertFalse(bookmarkProvider.showEditSuggestion)
    }

    @Test
    fun `GIVEN the non default general engine is selected WHEN history metadata is enabled THEN history and bookmark providers are not set`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showHistorySuggestionsForCurrentEngine = false,
            showAllHistorySuggestions = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showAllBookmarkSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showAllSyncedTabsSuggestions = false,
            showSessionSuggestionsForCurrentEngine = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { isGeneral } returns true
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val combinedHistoryProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNull(combinedHistoryProvider)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider }
        assertNull(bookmarkProvider)
    }

    @Test
    fun `GIVEN the non default general engine is selected WHEN history metadata is disabled THEN history and bookmark providers are not set`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showHistorySuggestionsForCurrentEngine = false,
            showAllHistorySuggestions = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showAllBookmarkSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showAllSyncedTabsSuggestions = false,
            showSessionSuggestionsForCurrentEngine = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { isGeneral } returns true
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val defaultHistoryProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider }
        assertNull(defaultHistoryProvider)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider }
        assertNull(bookmarkProvider)
    }

    @Test
    fun `GIVEN the non default non general engine is selected WHEN history metadata is enabled THEN suggestions are disabled in history and bookmark providers`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showAllHistorySuggestions = false,
            showAllBookmarkSuggestions = false,
            showAllSyncedTabsSuggestions = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { isGeneral } returns false
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val combinedHistoryProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider } as CombinedHistorySuggestionProvider
        assertNotNull(combinedHistoryProvider)
        assertFalse(combinedHistoryProvider.showEditSuggestion)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider } as BookmarksStorageSuggestionProvider
        assertNotNull(bookmarkProvider)
        assertFalse(bookmarkProvider.showEditSuggestion)
    }

    @Test
    fun `GIVEN the non default non general engine is selected WHEN history metadata is disabled THEN suggestions are disabled in history and bookmark providers`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showAllHistorySuggestions = false,
            showAllBookmarkSuggestions = false,
            showAllSyncedTabsSuggestions = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { isGeneral } returns false
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val defaultHistoryProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider } as HistoryStorageSuggestionProvider
        assertNotNull(defaultHistoryProvider)
        assertFalse(defaultHistoryProvider.showEditSuggestion)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider } as BookmarksStorageSuggestionProvider
        assertNotNull(bookmarkProvider)
        assertFalse(bookmarkProvider.showEditSuggestion)
    }

    @Test
    fun `GIVEN history is selected WHEN history metadata is enabled THEN suggestions are disabled in history provider, bookmark provider is not set`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showSearchTermHistory = false,
            showHistorySuggestionsForCurrentEngine = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showAllBookmarkSuggestions = false,
            showSearchSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showAllSyncedTabsSuggestions = false,
            showSessionSuggestionsForCurrentEngine = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val combinedHistoryProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider } as CombinedHistorySuggestionProvider
        assertNotNull(combinedHistoryProvider)
        assertFalse(combinedHistoryProvider.showEditSuggestion)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider }
        assertNull(bookmarkProvider)
    }

    @Test
    fun `GIVEN history is selected WHEN history metadata is disabled THEN suggestions are disabled in history provider, bookmark provider is not set`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showSearchTermHistory = false,
            showHistorySuggestionsForCurrentEngine = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showAllBookmarkSuggestions = false,
            showSearchSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showAllSyncedTabsSuggestions = false,
            showSessionSuggestionsForCurrentEngine = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val defaultHistoryProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider } as HistoryStorageSuggestionProvider
        assertNotNull(defaultHistoryProvider)
        assertFalse(defaultHistoryProvider.showEditSuggestion)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider }
        assertNull(bookmarkProvider)
    }

    @Test
    fun `GIVEN tab engine is selected WHEN history metadata is enabled THEN history and bookmark providers are not set`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showSearchTermHistory = false,
            showHistorySuggestionsForCurrentEngine = false,
            showAllHistorySuggestions = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showAllBookmarkSuggestions = false,
            showSearchSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showSessionSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Tabs(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val combinedHistoryProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNull(combinedHistoryProvider)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider }
        assertNull(bookmarkProvider)
    }

    @Test
    fun `GIVEN tab engine is selected WHEN history metadata is disabled THEN history and bookmark providers are not set`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showSearchTermHistory = false,
            showHistorySuggestionsForCurrentEngine = false,
            showAllHistorySuggestions = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showAllBookmarkSuggestions = false,
            showSearchSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showSessionSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Tabs(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val defaultHistoryProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider }
        assertNull(defaultHistoryProvider)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider }
        assertNull(bookmarkProvider)
    }

    @Test
    fun `GIVEN bookmarks engine is selected WHEN history metadata is enabled THEN history provider is not set, suggestions are disabled in bookmark provider`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showSearchTermHistory = false,
            showHistorySuggestionsForCurrentEngine = false,
            showAllHistorySuggestions = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showSearchSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showAllSyncedTabsSuggestions = false,
            showSessionSuggestionsForCurrentEngine = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Bookmarks(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val combinedHistoryProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNull(combinedHistoryProvider)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider } as BookmarksStorageSuggestionProvider
        assertNotNull(bookmarkProvider)
        assertFalse(bookmarkProvider.showEditSuggestion)
    }

    @Test
    fun `GIVEN bookmarks engine is selected WHEN history metadata is disabled THEN history provider is not set, suggestions are disabled in bookmark provider`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSearchShortcuts = false,
            showSearchTermHistory = false,
            showHistorySuggestionsForCurrentEngine = false,
            showAllHistorySuggestions = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showSearchSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showAllSyncedTabsSuggestions = false,
            showSessionSuggestionsForCurrentEngine = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Bookmarks(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val defaultHistoryProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider }
        assertNull(defaultHistoryProvider)

        val bookmarkProvider = result.firstOrNull { it is BookmarksStorageSuggestionProvider } as BookmarksStorageSuggestionProvider
        assertNotNull(bookmarkProvider)
        assertFalse(bookmarkProvider.showEditSuggestion)
    }

    @Test
    fun `GIVEN a search that should show filtered history WHEN history metadata is disabled and sponsored suggestions are enabled THEN return a history provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        val url = Uri.parse("test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllHistorySuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider }
        assertNotNull(historyProvider)
        assertNotNull((historyProvider as HistoryStorageSuggestionProvider).resultsUriFilter)
        assertEquals(AwesomeBarView.METADATA_SUGGESTION_LIMIT, historyProvider.getMaxNumberOfSuggestions())
    }

    @Test
    fun `GIVEN a search that should show filtered history WHEN history metadata is disabled and sponsored suggestions are disabled THEN return a history provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        val url = Uri.parse("test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllHistorySuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider }
        assertNotNull(historyProvider)
        assertNotNull((historyProvider as HistoryStorageSuggestionProvider).resultsUriFilter)
        assertEquals(AwesomeBarView.METADATA_SUGGESTION_LIMIT, historyProvider.getMaxNumberOfSuggestions())
    }

    @Test
    fun `GIVEN a search from the default engine WHEN configuring providers THEN add search action and search suggestions providers`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllHistorySuggestions = false,
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(1, result.filterIsInstance<SearchActionProvider>().size)
        assertEquals(1, result.filterIsInstance<SearchSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN a search from a shortcut engine WHEN configuring providers THEN add search action and search suggestions providers`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllHistorySuggestions = false,
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(1, result.filterIsInstance<SearchActionProvider>().size)
        assertEquals(1, result.filterIsInstance<SearchSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN searches from other than default and shortcut engines WHEN configuring providers THEN don't add search action and search suggestion providers`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings

        val historyState = getSearchProviderState(
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
        )
        val bookmarksState = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Bookmarks(mockk(relaxed = true)),
        )
        val tabsState = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Tabs(mockk(relaxed = true)),
        )
        val noneState = getSearchProviderState()

        val historyResult = awesomeBarView.getProvidersToAdd(historyState)
        val bookmarksResult = awesomeBarView.getProvidersToAdd(bookmarksState)
        val tabsResult = awesomeBarView.getProvidersToAdd(tabsState)
        val noneResult = awesomeBarView.getProvidersToAdd(noneState)
        val allResults = historyResult + bookmarksResult + tabsResult + noneResult

        assertEquals(0, allResults.filterIsInstance<SearchActionProvider>().size)
        assertEquals(0, allResults.filterIsInstance<SearchSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN normal browsing mode and needing to show all local tabs suggestions and sponsored suggestions are enabled WHEN configuring providers THEN add the tabs provider`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSessionSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SessionSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNull(localSessionsProviders[0].resultsUriFilter)
    }

    @Test
    fun `GIVEN normal browsing mode and needing to show all local tabs suggestions and sponsored suggestions are disabled WHEN configuring providers THEN add the tabs provider`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSessionSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SessionSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNull(localSessionsProviders[0].resultsUriFilter)
    }

    @Test
    fun `GIVEN normal browsing mode and needing to show filtered local tabs suggestions WHEN configuring providers THEN add the tabs provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SessionSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNotNull(localSessionsProviders[0].resultsUriFilter)
    }

    @Test
    fun `GIVEN private browsing mode and needing to show tabs suggestions WHEN configuring providers THEN don't add the tabs provider`() {
        val privateAppStore = AppStore(AppState(mode = BrowsingMode.Private))
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { any<Activity>().components.appStore } returns privateAppStore
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(0, result.filterIsInstance<SessionSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN needing to show all synced tabs suggestions and sponsored suggestions are enabled WHEN configuring providers THEN add the synced tabs provider with a sponsored filter`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSyncedTabsSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SyncedTabsStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNotNull(localSessionsProviders[0].resultsUrlFilter)
    }

    @Test
    fun `GIVEN needing to show filtered synced tabs suggestions and sponsored suggestions are enabled WHEN configuring providers THEN add the synced tabs provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllSyncedTabsSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SyncedTabsStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNotNull(localSessionsProviders[0].resultsUrlFilter)
    }

    @Test
    fun `GIVEN needing to show all synced tabs suggestions and sponsored suggestions are disabled WHEN configuring providers THEN add the synced tabs provider`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showSyncedTabsSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SyncedTabsStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNull(localSessionsProviders[0].resultsUrlFilter)
    }

    @Test
    fun `GIVEN needing to show all bookmarks suggestions and sponsored suggestions are enabled WHEN configuring providers THEN add the bookmarks provider with a sponsored filter`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showBookmarksSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<BookmarksStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNotNull(localSessionsProviders[0].resultsUriFilter)
    }

    @Test
    fun `GIVEN needing to show all bookmarks suggestions and sponsored suggestions are disabled WHEN configuring providers THEN add the bookmarks provider`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showBookmarksSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<BookmarksStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNull(localSessionsProviders[0].resultsUriFilter)
    }

    @Test
    fun `GIVEN needing to show filtered bookmarks suggestions WHEN configuring providers THEN add the bookmarks provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllBookmarkSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<BookmarksStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNotNull(localSessionsProviders[0].resultsUriFilter)
    }

    @Test
    fun `GIVEN a search is made by the user WHEN configuring providers THEN search engine suggestion provider should always be added`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(1, result.filterIsInstance<SearchEngineSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN a search from the default engine with all suggestions asked and sponsored suggestions are enabled WHEN configuring providers THEN add them all`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProviders: List<HistoryStorageSuggestionProvider> = result.filterIsInstance<HistoryStorageSuggestionProvider>()
        assertEquals(2, historyProviders.size)
        assertNotNull(historyProviders[0].resultsUriFilter) // the general history provider
        assertNotNull(historyProviders[1].resultsUriFilter) // the filtered history provider
        val bookmarksProviders: List<BookmarksStorageSuggestionProvider> = result.filterIsInstance<BookmarksStorageSuggestionProvider>()
        assertEquals(2, bookmarksProviders.size)
        assertNotNull(bookmarksProviders[0].resultsUriFilter) // the general bookmarks provider
        assertNotNull(bookmarksProviders[1].resultsUriFilter) // the filtered bookmarks provider
        assertEquals(1, result.filterIsInstance<SearchActionProvider>().size)
        assertEquals(1, result.filterIsInstance<SearchSuggestionProvider>().size)
        val syncedTabsProviders: List<SyncedTabsStorageSuggestionProvider> = result.filterIsInstance<SyncedTabsStorageSuggestionProvider>()
        assertEquals(2, syncedTabsProviders.size)
        assertNotNull(syncedTabsProviders[0].resultsUrlFilter) // the general synced tabs provider
        assertNotNull(syncedTabsProviders[1].resultsUrlFilter) // the filtered synced tabs provider
        val localTabsProviders: List<SessionSuggestionProvider> = result.filterIsInstance<SessionSuggestionProvider>()
        assertEquals(2, localTabsProviders.size)
        assertNull(localTabsProviders[0].resultsUriFilter) // the general tabs provider
        assertNotNull(localTabsProviders[1].resultsUriFilter) // the filtered tabs provider
        assertEquals(1, result.filterIsInstance<SearchEngineSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN a search from the default engine with all suggestions asked and sponsored suggestions are disabled WHEN configuring providers THEN add them all`() {
        val settings: Settings = mockk(relaxed = true)
        val url = Uri.parse("https://www.test.com")
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
            showSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProviders: List<HistoryStorageSuggestionProvider> = result.filterIsInstance<HistoryStorageSuggestionProvider>()
        assertEquals(2, historyProviders.size)
        assertNull(historyProviders[0].resultsUriFilter) // the general history provider
        assertNotNull(historyProviders[1].resultsUriFilter) // the filtered history provider
        val bookmarksProviders: List<BookmarksStorageSuggestionProvider> = result.filterIsInstance<BookmarksStorageSuggestionProvider>()
        assertEquals(2, bookmarksProviders.size)
        assertNull(bookmarksProviders[0].resultsUriFilter) // the general bookmarks provider
        assertNotNull(bookmarksProviders[1].resultsUriFilter) // the filtered bookmarks provider
        assertEquals(1, result.filterIsInstance<SearchActionProvider>().size)
        assertEquals(1, result.filterIsInstance<SearchSuggestionProvider>().size)
        val syncedTabsProviders: List<SyncedTabsStorageSuggestionProvider> = result.filterIsInstance<SyncedTabsStorageSuggestionProvider>()
        assertEquals(2, syncedTabsProviders.size)
        assertNull(syncedTabsProviders[0].resultsUrlFilter) // the general synced tabs provider
        assertNotNull(syncedTabsProviders[1].resultsUrlFilter) // the filtered synced tabs provider
        val localTabsProviders: List<SessionSuggestionProvider> = result.filterIsInstance<SessionSuggestionProvider>()
        assertEquals(2, localTabsProviders.size)
        assertNull(localTabsProviders[0].resultsUriFilter) // the general tabs provider
        assertNotNull(localTabsProviders[1].resultsUriFilter) // the filtered tabs provider
        assertEquals(1, result.filterIsInstance<SearchEngineSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN a search from the default engine with no suggestions asked WHEN configuring providers THEN add only search engine suggestion provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showHistorySuggestionsForCurrentEngine = false,
            showSearchShortcuts = false,
            showAllHistorySuggestions = false,
            showBookmarksSuggestionsForCurrentEngine = false,
            showAllBookmarkSuggestions = false,
            showSearchSuggestions = false,
            showSyncedTabsSuggestionsForCurrentEngine = false,
            showAllSyncedTabsSuggestions = false,
            showSessionSuggestionsForCurrentEngine = false,
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(0, result.filterIsInstance<HistoryStorageSuggestionProvider>().size)
        assertEquals(0, result.filterIsInstance<BookmarksStorageSuggestionProvider>().size)
        assertEquals(0, result.filterIsInstance<SearchActionProvider>().size)
        assertEquals(0, result.filterIsInstance<SearchSuggestionProvider>().size)
        assertEquals(0, result.filterIsInstance<SyncedTabsStorageSuggestionProvider>().size)
        assertEquals(0, result.filterIsInstance<SessionSuggestionProvider>().size)
        assertEquals(1, result.filterIsInstance<SearchEngineSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN sponsored suggestions are enabled WHEN configuring providers THEN add the Firefox Suggest suggestion provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        val awesomeBarView = AwesomeBarView(
            activity = activity,
            interactor = mockk(),
            view = mockk(),
            fromHomeFragment = false,
        )
        val state = getSearchProviderState(
            showSponsoredSuggestions = true,
            showNonSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val fxSuggestProvider = result.firstOrNull { it is FxSuggestSuggestionProvider }
        assertNotNull(fxSuggestProvider)
    }

    @Test
    fun `GIVEN non-sponsored suggestions are enabled WHEN configuring providers THEN add the Firefox Suggest suggestion provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        val awesomeBarView = AwesomeBarView(
            activity = activity,
            interactor = mockk(),
            view = mockk(),
            fromHomeFragment = false,
        )
        val state = getSearchProviderState(
            showSponsoredSuggestions = false,
            showNonSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val fxSuggestProvider = result.firstOrNull { it is FxSuggestSuggestionProvider }
        assertNotNull(fxSuggestProvider)
    }

    @Test
    fun `GIVEN sponsored and non-sponsored suggestions are enabled WHEN configuring providers THEN add the Firefox Suggest suggestion provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        val awesomeBarView = AwesomeBarView(
            activity = activity,
            interactor = mockk(),
            view = mockk(),
            fromHomeFragment = false,
        )
        val state = getSearchProviderState(
            showSponsoredSuggestions = true,
            showNonSponsoredSuggestions = true,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val fxSuggestProvider = result.firstOrNull { it is FxSuggestSuggestionProvider }
        assertNotNull(fxSuggestProvider)
    }

    @Test
    fun `GIVEN sponsored and non-sponsored suggestions are disabled WHEN configuring providers THEN don't add the Firefox Suggest suggestion provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        val awesomeBarView = AwesomeBarView(
            activity = activity,
            interactor = mockk(),
            view = mockk(),
            fromHomeFragment = false,
        )
        val state = getSearchProviderState(
            showSponsoredSuggestions = false,
            showNonSponsoredSuggestions = false,
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val fxSuggestProvider = result.firstOrNull { it is FxSuggestSuggestionProvider }
        assertNull(fxSuggestProvider)
    }

    @Test
    fun `GIVEN a valid search engine and history metadata enabled WHEN creating a history provider for that engine THEN return a history metadata provider with engine filter`() {
        val settings: Settings = mockk {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true))
        val state = getSearchProviderState(searchEngineSource = searchEngineSource)

        val result = awesomeBarView.getHistoryProvider(
            filter = awesomeBarView.getFilterForCurrentEngineResults(state),
        )

        assertNotNull(result)
        assertTrue(result is CombinedHistorySuggestionProvider)
        assertNotNull((result as CombinedHistorySuggestionProvider).resultsUriFilter)
        assertEquals(AwesomeBarView.METADATA_SUGGESTION_LIMIT, result.getMaxNumberOfSuggestions())
    }

    @Test
    fun `GIVEN a valid search engine and history metadata disabled WHEN creating a history provider for that engine THEN return a history metadata provider with an engine filter`() {
        val settings: Settings = mockk {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true))
        val state = getSearchProviderState(
            searchEngineSource = searchEngineSource,
        )

        val result = awesomeBarView.getHistoryProvider(
            filter = awesomeBarView.getFilterForCurrentEngineResults(state),
        )

        assertNotNull(result)
        assertTrue(result is HistoryStorageSuggestionProvider)
        assertNotNull((result as HistoryStorageSuggestionProvider).resultsUriFilter)
        assertEquals(AwesomeBarView.METADATA_SUGGESTION_LIMIT, result.getMaxNumberOfSuggestions())
    }

    @Test
    fun `GIVEN a search engine is not available WHEN asking for a search term provider THEN return null`() {
        val searchEngineSource: SearchEngineSource = SearchEngineSource.None

        val result = awesomeBarView.getSearchTermSuggestionsProvider(searchEngineSource)

        assertNull(result)
    }

    @Test
    fun `GIVEN a search engine is available WHEN asking for a search term provider THEN return a valid provider`() {
        val searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true))

        val result = awesomeBarView.getSearchTermSuggestionsProvider(searchEngineSource)

        assertTrue(result is SearchTermSuggestionsProvider)
    }

    @Test
    fun `GIVEN the default search engine WHEN asking for a search term provider THEN the provider should have a suggestions header`() {
        val engine: SearchEngine = mockk {
            every { name } returns "Test"
        }
        val searchEngineSource = SearchEngineSource.Default(engine)
        every { AwesomeBarView.Companion.getString(any(), any(), any()) } answers {
            "Search Test"
        }

        mockkStatic("mozilla.components.browser.state.state.SearchStateKt") {
            every { any<SearchState>().selectedOrDefaultSearchEngine } returns engine

            val result = awesomeBarView.getSearchTermSuggestionsProvider(searchEngineSource)

            assertTrue(result is SearchTermSuggestionsProvider)
            assertEquals("Search Test", result?.groupTitle())
        }
    }

    @Test
    fun `GIVEN a shortcut search engine selected WHEN asking for a search term provider THEN the provider should not have a suggestions header`() {
        val defaultEngine: SearchEngine = mockk {
            every { name } returns "Test"
        }
        val otherEngine: SearchEngine = mockk {
            every { name } returns "Other"
        }
        val searchEngineSource = SearchEngineSource.Shortcut(otherEngine)
        every { AwesomeBarView.Companion.getString(any(), any(), any()) } answers {
            "Search Test"
        }

        mockkStatic("mozilla.components.browser.state.state.SearchStateKt") {
            every { any<SearchState>().selectedOrDefaultSearchEngine } returns defaultEngine

            val result = awesomeBarView.getSearchTermSuggestionsProvider(searchEngineSource)

            assertTrue(result is SearchTermSuggestionsProvider)
            assertNull(result?.groupTitle())
        }
    }

    @Test
    fun `GIVEN the default search engine is unknown WHEN asking for a search term provider THEN the provider should not have a suggestions header`() {
        val defaultEngine: SearchEngine? = null
        val otherEngine: SearchEngine = mockk {
            every { name } returns "Other"
        }
        val searchEngineSource = SearchEngineSource.Shortcut(otherEngine)
        every { AwesomeBarView.Companion.getString(any(), any(), any()) } answers {
            "Search Test"
        }

        mockkStatic("mozilla.components.browser.state.state.SearchStateKt") {
            every { any<SearchState>().selectedOrDefaultSearchEngine } returns defaultEngine

            val result = awesomeBarView.getSearchTermSuggestionsProvider(searchEngineSource)

            assertTrue(result is SearchTermSuggestionsProvider)
            assertNull(result?.groupTitle())
        }
    }

    @Test
    fun `GIVEN history search term suggestions disabled WHEN getting suggestions providers THEN don't search term provider of past searches`() {
        every { activity.settings() } returns mockk(relaxed = true)
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
            showSearchTermHistory = false,
        )
        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(0, result.filterIsInstance<SearchTermSuggestionsProvider>().size)
    }

    @Test
    fun `GIVEN history search term suggestions enabled WHEN getting suggestions providers THEN add a search term provider of past searches`() {
        every { activity.settings() } returns mockk(relaxed = true)
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
            showSearchTermHistory = true,
        )
        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(1, result.filterIsInstance<SearchTermSuggestionsProvider>().size)
    }

    @Test
    fun `GIVEN sponsored suggestions are enabled WHEN getting a filter to exclude sponsored suggestions THEN return the filter`() {
        every { activity.settings() } returns mockk(relaxed = true) {
            every { frecencyFilterQuery } returns "query=value"
        }
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
            showSponsoredSuggestions = true,
        )
        val filter = awesomeBarView.getFilterToExcludeSponsoredResults(state)

        assertEquals(AwesomeBarView.SearchResultFilter.ExcludeSponsored("query=value"), filter)
    }

    @Test
    fun `GIVEN sponsored suggestions are disabled WHEN getting a filter to exclude sponsored suggestions THEN return null`() {
        every { activity.settings() } returns mockk(relaxed = true) {
            every { frecencyFilterQuery } returns "query=value"
        }
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
            showSponsoredSuggestions = false,
        )
        val filter = awesomeBarView.getFilterToExcludeSponsoredResults(state)

        assertNull(filter)
    }

    @Test
    fun `GIVEN a sponsored query parameter and a sponsored filter WHEN a URL contains the sponsored query parameter THEN that URL should be excluded`() {
        every { activity.settings() } returns mockk(relaxed = true) {
            every { frecencyFilterQuery } returns "query=value"
        }
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
            showSponsoredSuggestions = true,
        )
        val filter = requireNotNull(awesomeBarView.getFilterToExcludeSponsoredResults(state))

        assertFalse(filter.shouldIncludeUri(Uri.parse("http://example.com?query=value")))
        assertFalse(filter.shouldIncludeUri(Uri.parse("http://example.com/a?query=value")))
        assertFalse(filter.shouldIncludeUri(Uri.parse("http://example.com/a?b=c&query=value")))
        assertFalse(filter.shouldIncludeUri(Uri.parse("http://example.com/a?b=c&query=value&d=e")))

        assertFalse(filter.shouldIncludeUrl("http://example.com?query=value"))
        assertFalse(filter.shouldIncludeUrl("http://example.com/a?query=value"))
        assertFalse(filter.shouldIncludeUrl("http://example.com/a?b=c&query=value"))
        assertFalse(filter.shouldIncludeUrl("http://example.com/a?b=c&query=value&d=e"))
    }

    @Test
    fun `GIVEN a sponsored query parameter and a sponsored filter WHEN a URL does not contain the sponsored query parameter THEN that URL should be included`() {
        every { activity.settings() } returns mockk(relaxed = true) {
            every { frecencyFilterQuery } returns "query=value"
        }
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
            showSponsoredSuggestions = true,
        )
        val filter = requireNotNull(awesomeBarView.getFilterToExcludeSponsoredResults(state))

        assertTrue(filter.shouldIncludeUri(Uri.parse("http://example.com")))
        assertTrue(filter.shouldIncludeUri(Uri.parse("http://example.com?query")))
        assertTrue(filter.shouldIncludeUri(Uri.parse("http://example.com/a")))
        assertTrue(filter.shouldIncludeUri(Uri.parse("http://example.com/a?b")))
        assertTrue(filter.shouldIncludeUri(Uri.parse("http://example.com/a?b&c=d")))

        assertTrue(filter.shouldIncludeUrl("http://example.com"))
        assertTrue(filter.shouldIncludeUrl("http://example.com?query"))
        assertTrue(filter.shouldIncludeUrl("http://example.com/a"))
        assertTrue(filter.shouldIncludeUrl("http://example.com/a?b"))
        assertTrue(filter.shouldIncludeUrl("http://example.com/a?b&c=d"))
    }

    @Test
    fun `GIVEN an engine with a results URL and an engine filter WHEN a URL matches the results URL THEN that URL should be included`() {
        val url = Uri.parse("http://test.com")
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
        )

        val filter = requireNotNull(awesomeBarView.getFilterForCurrentEngineResults(state))

        assertTrue(filter.shouldIncludeUri(Uri.parse("http://test.com")))
        assertTrue(filter.shouldIncludeUri(Uri.parse("http://test.com/a")))
        assertTrue(filter.shouldIncludeUri(Uri.parse("http://mobile.test.com")))
        assertTrue(filter.shouldIncludeUri(Uri.parse("http://mobile.test.com/a")))

        assertTrue(filter.shouldIncludeUrl("http://test.com"))
        assertTrue(filter.shouldIncludeUrl("http://test.com/a"))
    }

    @Test
    fun `GIVEN an engine with a results URL and an engine filter WHEN a URL does not match the results URL THEN that URL should be excluded`() {
        val url = Uri.parse("http://test.com")
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl } returns url
                },
            ),
        )

        val filter = requireNotNull(awesomeBarView.getFilterForCurrentEngineResults(state))

        assertFalse(filter.shouldIncludeUri(Uri.parse("http://other.com")))
        assertFalse(filter.shouldIncludeUri(Uri.parse("http://subdomain.test.com")))

        assertFalse(filter.shouldIncludeUrl("http://mobile.test.com"))
    }
}

/**
 * Get a default [SearchProviderState] that by default will ask for all types of suggestions.
 */
private fun getSearchProviderState(
    showSearchShortcuts: Boolean = true,
    showSearchTermHistory: Boolean = true,
    showHistorySuggestionsForCurrentEngine: Boolean = true,
    showAllHistorySuggestions: Boolean = true,
    showBookmarksSuggestionsForCurrentEngine: Boolean = true,
    showAllBookmarkSuggestions: Boolean = true,
    showSearchSuggestions: Boolean = true,
    showSyncedTabsSuggestionsForCurrentEngine: Boolean = true,
    showAllSyncedTabsSuggestions: Boolean = true,
    showSessionSuggestionsForCurrentEngine: Boolean = true,
    showAllSessionSuggestions: Boolean = true,
    searchEngineSource: SearchEngineSource = SearchEngineSource.None,
    showSponsoredSuggestions: Boolean = true,
    showNonSponsoredSuggestions: Boolean = true,
) = SearchProviderState(
    showSearchShortcuts = showSearchShortcuts,
    showSearchTermHistory = showSearchTermHistory,
    showHistorySuggestionsForCurrentEngine = showHistorySuggestionsForCurrentEngine,
    showAllHistorySuggestions = showAllHistorySuggestions,
    showBookmarksSuggestionsForCurrentEngine = showBookmarksSuggestionsForCurrentEngine,
    showAllBookmarkSuggestions = showAllBookmarkSuggestions,
    showSearchSuggestions = showSearchSuggestions,
    showSyncedTabsSuggestionsForCurrentEngine = showSyncedTabsSuggestionsForCurrentEngine,
    showAllSyncedTabsSuggestions = showAllSyncedTabsSuggestions,
    showSessionSuggestionsForCurrentEngine = showSessionSuggestionsForCurrentEngine,
    showAllSessionSuggestions = showAllSessionSuggestions,
    showSponsoredSuggestions = showSponsoredSuggestions,
    showNonSponsoredSuggestions = showNonSponsoredSuggestions,
    searchEngineSource = searchEngineSource,
)
