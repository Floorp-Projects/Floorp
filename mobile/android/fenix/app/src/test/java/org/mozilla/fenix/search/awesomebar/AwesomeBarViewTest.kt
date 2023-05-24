/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import android.app.Activity
import android.graphics.drawable.VectorDrawable
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
import org.mozilla.fenix.components.Core.Companion
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.awesomebar.AwesomeBarView.SearchProviderState
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class AwesomeBarViewTest {
    private var activity: HomeActivity = mockk(relaxed = true)
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
        every { any<Activity>().components.core.store.state.search } returns mockk(relaxed = true)
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
    fun `GIVEN a search from history and history metadata enabled WHEN setting the providers THEN set more suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
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
    fun `GIVEN a search from history and history metadata disabled WHEN setting the providers THEN set more suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.History(mockk(relaxed = true)),
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
    fun `GIVEN a search not from history and history metadata enabled WHEN setting the providers THEN set less suggestions to be shown`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true)),
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
    fun `GIVEN a search that should show filtered history WHEN history metadata is enabled THEN return a history metadata provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllHistorySuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is CombinedHistorySuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals("test", (historyProvider as CombinedHistorySuggestionProvider).resultsHostFilter)
        assertEquals(AwesomeBarView.METADATA_SUGGESTION_LIMIT, historyProvider.getMaxNumberOfSuggestions())
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
    fun `GIVEN a search that should show filtered history WHEN history metadata is disabled THEN return a history provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true) {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            showAllHistorySuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProvider = result.firstOrNull { it is HistoryStorageSuggestionProvider }
        assertNotNull(historyProvider)
        assertEquals("test", (historyProvider as HistoryStorageSuggestionProvider).resultsHostFilter)
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
    fun `GIVEN normal browsing mode and needing to show all local tabs suggestions WHEN configuring providers THEN add the tabs provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
        val state = getSearchProviderState(
            showSessionSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SessionSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNull(localSessionsProviders[0].resultsHostFilter)
    }

    @Test
    fun `GIVEN normal browsing mode and needing to show filtered local tabs suggestions WHEN configuring providers THEN add the tabs provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
        val state = getSearchProviderState(
            showAllSessionSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SessionSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertEquals("test", localSessionsProviders[0].resultsHostFilter)
    }

    @Test
    fun `GIVEN private browsing mode and needing to show tabs suggestions WHEN configuring providers THEN don't add the tabs provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Private
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(0, result.filterIsInstance<SessionSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN needing to show all synced tabs suggestions WHEN configuring providers THEN add the synced tabs provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
        val state = getSearchProviderState(
            showSyncedTabsSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SyncedTabsStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNull(localSessionsProviders[0].resultsHostFilter)
    }

    @Test
    fun `GIVEN needing to show filtered synced tabs suggestions WHEN configuring providers THEN add the synced tabs provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
        val state = getSearchProviderState(
            showAllSyncedTabsSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<SyncedTabsStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertEquals("test", localSessionsProviders[0].resultsHostFilter)
    }

    @Test
    fun `GIVEN needing to show all bookmarks suggestions WHEN configuring providers THEN add the bookmarks provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
        val state = getSearchProviderState(
            showBookmarksSuggestionsForCurrentEngine = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<BookmarksStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertNull(localSessionsProviders[0].resultsHostFilter)
    }

    @Test
    fun `GIVEN needing to show filtered bookmarks suggestions WHEN configuring providers THEN add the bookmarks provider with an engine filter`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
        val state = getSearchProviderState(
            showAllBookmarkSuggestions = false,
            searchEngineSource = SearchEngineSource.Shortcut(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val localSessionsProviders = result.filterIsInstance<BookmarksStorageSuggestionProvider>()
        assertEquals(1, localSessionsProviders.size)
        assertEquals("test", localSessionsProviders[0].resultsHostFilter)
    }

    @Test
    fun `GIVEN unified search feature is enabled WHEN configuring providers THEN don't add the engine suggestions provider`() {
        val settings: Settings = mockk(relaxed = true) {
            every { showUnifiedSearchFeature } returns true
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(0, result.filterIsInstance<SearchEngineSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN unified search feature is disabled WHEN configuring providers THEN add the engine suggestions provider`() {
        val settings: Settings = mockk(relaxed = true) {
            every { showUnifiedSearchFeature } returns false
        }
        every { activity.settings() } returns settings
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(1, result.filterIsInstance<SearchEngineSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN a search from the default engine with all suggestions asked WHEN configuring providers THEN add them all`() {
        val settings: Settings = mockk(relaxed = true) {
            every { showUnifiedSearchFeature } returns false
        }
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Default(
                mockk(relaxed = true) {
                    every { resultsUrl.host } returns "test"
                },
            ),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProviders: List<HistoryStorageSuggestionProvider> = result.filterIsInstance<HistoryStorageSuggestionProvider>()
        assertEquals(2, historyProviders.size)
        assertNull(historyProviders[0].resultsHostFilter) // the general history provider
        assertNotNull(historyProviders[1].resultsHostFilter) // the filtered history provider
        val bookmarksProviders: List<BookmarksStorageSuggestionProvider> = result.filterIsInstance<BookmarksStorageSuggestionProvider>()
        assertEquals(2, bookmarksProviders.size)
        assertNull(bookmarksProviders[0].resultsHostFilter) // the general bookmarks provider
        assertEquals("test", bookmarksProviders[1].resultsHostFilter) // the filtered bookmarks provider
        assertEquals(1, result.filterIsInstance<SearchActionProvider>().size)
        assertEquals(1, result.filterIsInstance<SearchSuggestionProvider>().size)
        val syncedTabsProviders: List<SyncedTabsStorageSuggestionProvider> = result.filterIsInstance<SyncedTabsStorageSuggestionProvider>()
        assertEquals(2, syncedTabsProviders.size)
        assertNull(syncedTabsProviders[0].resultsHostFilter) // the general synced tabs provider
        assertEquals("test", syncedTabsProviders[1].resultsHostFilter) // the filtered synced tabs provider
        val localTabsProviders: List<SessionSuggestionProvider> = result.filterIsInstance<SessionSuggestionProvider>()
        assertEquals(2, localTabsProviders.size)
        assertNull(localTabsProviders[0].resultsHostFilter) // the general tabs provider
        assertEquals("test", localTabsProviders[1].resultsHostFilter) // the filtered tabs provider
        assertEquals(1, result.filterIsInstance<SearchEngineSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN a search from the default engine with no suggestions asked WHEN configuring providers THEN don't add any provider`() {
        val settings: Settings = mockk(relaxed = true) {
            every { showUnifiedSearchFeature } returns true
        }
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
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
        assertEquals(0, result.filterIsInstance<SearchEngineSuggestionProvider>().size)
    }

    @Test
    fun `GIVEN the current search engine's url is not known WHEN creating a history provider for that engine THEN return null`() {
        val engineSource = SearchEngineSource.None

        val result = awesomeBarView.getHistoryProvidersForSearchEngine(engineSource)

        assertNull(result)
    }

    @Test
    fun `GIVEN a valid search engine and history metadata enabled WHEN creating a history provider for that engine THEN return a history metadata provider with engine filter`() {
        val settings: Settings = mockk {
            every { historyMetadataUIFeature } returns true
        }
        every { activity.settings() } returns settings
        val searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true))

        val result = awesomeBarView.getHistoryProvidersForSearchEngine(searchEngineSource)

        assertNotNull(result)
        assertTrue(result is CombinedHistorySuggestionProvider)
        assertNotNull((result as CombinedHistorySuggestionProvider).resultsHostFilter)
        assertEquals(AwesomeBarView.METADATA_SUGGESTION_LIMIT, result.getMaxNumberOfSuggestions())
    }

    @Test
    fun `GIVEN a valid search engine and history metadata disabled WHEN creating a history provider for that engine THEN return a history metadata provider with engine filter`() {
        val settings: Settings = mockk {
            every { historyMetadataUIFeature } returns false
        }
        every { activity.settings() } returns settings
        val searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true))

        val result = awesomeBarView.getHistoryProvidersForSearchEngine(searchEngineSource)

        assertNotNull(result)
        assertTrue(result is HistoryStorageSuggestionProvider)
        assertNotNull((result as HistoryStorageSuggestionProvider).resultsHostFilter)
        assertEquals(AwesomeBarView.METADATA_SUGGESTION_LIMIT, result.getMaxNumberOfSuggestions())
    }

    @Test
    fun `GIVEN a filter is required WHEN configuring a bookmarks provider THEN include a url filter`() {
        assertNotNull(
            awesomeBarView.getBookmarksProvider(
                searchEngineSource = mockk(relaxed = true),
            ),
        )

        assertNotNull(
            awesomeBarView.getBookmarksProvider(
                searchEngineSource = mockk(relaxed = true),
                filterByCurrentEngine = true,
            ),
        )
    }

    @Test
    fun `GIVEN a filter is required WHEN configuring a synced tabs provider THEN include a url filter`() {
        assertNotNull(
            awesomeBarView.getSyncedTabsProvider(
                searchEngineSource = mockk(relaxed = true),
            ),
        )

        assertNotNull(
            awesomeBarView.getSyncedTabsProvider(
                searchEngineSource = mockk(relaxed = true),
                filterByCurrentEngine = true,
            ),
        )
    }

    @Test
    fun `GIVEN a filter is required WHEN configuring a local tabs provider THEN include a url filter`() {
        assertNotNull(
            awesomeBarView.getLocalTabsProvider(
                searchEngineSource = mockk(relaxed = true),
            ),
        )

        assertNotNull(
            awesomeBarView.getLocalTabsProvider(
                searchEngineSource = mockk(relaxed = true),
                filterByCurrentEngine = true,
            ),
        )
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
    searchEngineSource = searchEngineSource,
)
