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
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.CombinedHistorySuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchActionProvider
import mozilla.components.feature.awesomebar.provider.SearchEngineSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.syncedtabs.SyncedTabsStorageSuggestionProvider
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.junit.After
import org.junit.Assert.assertEquals
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
    private val activity: HomeActivity = mockk(relaxed = true)
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
    fun `GIVEN normal browsing mode and needing to show tabs suggestions WHEN configuring providers THEN add the tabs provider`() {
        val settings: Settings = mockk(relaxed = true)
        every { activity.settings() } returns settings
        every { activity.browsingModeManager.mode } returns BrowsingMode.Normal
        val state = getSearchProviderState(
            searchEngineSource = SearchEngineSource.Shortcut(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        assertEquals(1, result.filterIsInstance<SessionSuggestionProvider>().size)
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
            searchEngineSource = SearchEngineSource.Default(mockk(relaxed = true)),
        )

        val result = awesomeBarView.getProvidersToAdd(state)

        val historyProviders: List<HistoryStorageSuggestionProvider> = result.filterIsInstance<HistoryStorageSuggestionProvider>()
        assertEquals(2, historyProviders.size)
        assertNull(historyProviders[0].resultsHostFilter) // the general history provider
        assertNotNull(historyProviders[1].resultsHostFilter) // the filtered history provider
        assertEquals(1, result.filterIsInstance<BookmarksStorageSuggestionProvider>().size)
        assertEquals(1, result.filterIsInstance<SearchActionProvider>().size)
        assertEquals(1, result.filterIsInstance<SearchSuggestionProvider>().size)
        assertEquals(1, result.filterIsInstance<SyncedTabsStorageSuggestionProvider>().size)
        assertEquals(1, result.filterIsInstance<SessionSuggestionProvider>().size)
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
            showBookmarkSuggestions = false,
            showSearchSuggestions = false,
            showSyncedTabsSuggestions = false,
            showSessionSuggestions = false,
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
}

/**
 * Get a default [SearchProviderState] that by default will ask for all types of suggestions.
 */
private fun getSearchProviderState(
    showSearchShortcuts: Boolean = true,
    showHistorySuggestionsForCurrentEngine: Boolean = true,
    showAllHistorySuggestions: Boolean = true,
    showBookmarkSuggestions: Boolean = true,
    showSearchSuggestions: Boolean = true,
    showSyncedTabsSuggestions: Boolean = true,
    showSessionSuggestions: Boolean = true,
    searchEngineSource: SearchEngineSource = SearchEngineSource.None,
) = SearchProviderState(
    showSearchShortcuts = showSearchShortcuts,
    showHistorySuggestionsForCurrentEngine = showHistorySuggestionsForCurrentEngine,
    showAllHistorySuggestions = showAllHistorySuggestions,
    showBookmarkSuggestions = showBookmarkSuggestions,
    showSearchSuggestions = showSearchSuggestions,
    showSyncedTabsSuggestions = showSyncedTabsSuggestions,
    showSessionSuggestions = showSessionSuggestions,
    searchEngineSource = searchEngineSource,
)
