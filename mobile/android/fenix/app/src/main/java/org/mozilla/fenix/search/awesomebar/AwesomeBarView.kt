/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.core.graphics.BlendModeColorFilterCompat.createBlendModeColorFilterCompat
import androidx.core.graphics.BlendModeCompat.SRC_IN
import androidx.core.graphics.drawable.toBitmap
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.CombinedHistorySuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchActionProvider
import mozilla.components.feature.awesomebar.provider.SearchEngineSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.syncedtabs.DeviceIndicators
import mozilla.components.feature.syncedtabs.SyncedTabsStorageSuggestionProvider
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Core.Companion.METADATA_HISTORY_SUGGESTION_LIMIT
import org.mozilla.fenix.components.Core.Companion.METADATA_SHORTCUT_SUGGESTION_LIMIT
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.search.SearchEngineSource
import org.mozilla.fenix.search.SearchFragmentState

/**
 * View that contains and configures the BrowserAwesomeBar
 */
@Suppress("LargeClass")
class AwesomeBarView(
    private val activity: HomeActivity,
    val interactor: AwesomeBarInteractor,
    val view: AwesomeBarWrapper,
    fromHomeFragment: Boolean,
) {
    private val sessionProvider: SessionSuggestionProvider
    private val historyStorageProvider: HistoryStorageSuggestionProvider
    private val combinedHistoryProvider: CombinedHistorySuggestionProvider
    private val shortcutsEnginePickerProvider: ShortcutsSuggestionProvider
    private val bookmarksStorageSuggestionProvider: BookmarksStorageSuggestionProvider
    private val syncedTabsStorageSuggestionProvider: SyncedTabsStorageSuggestionProvider
    private val defaultSearchSuggestionProvider: SearchSuggestionProvider
    private val defaultSearchActionProvider: SearchActionProvider
    private val searchEngineSuggestionProvider: SearchEngineSuggestionProvider
    private val searchSuggestionProviderMap: MutableMap<SearchEngine, List<AwesomeBar.SuggestionProvider>>

    private val loadUrlUseCase = object : SessionUseCases.LoadUrlUseCase {
        override fun invoke(
            url: String,
            flags: EngineSession.LoadUrlFlags,
            additionalHeaders: Map<String, String>?,
        ) {
            interactor.onUrlTapped(url, flags)
        }
    }

    private val searchUseCase = object : SearchUseCases.SearchUseCase {
        override fun invoke(
            searchTerms: String,
            searchEngine: SearchEngine?,
            parentSessionId: String?,
        ) {
            interactor.onSearchTermsTapped(searchTerms)
        }
    }

    private val shortcutSearchUseCase = object : SearchUseCases.SearchUseCase {
        override fun invoke(
            searchTerms: String,
            searchEngine: SearchEngine?,
            parentSessionId: String?,
        ) {
            interactor.onSearchTermsTapped(searchTerms)
        }
    }

    private val selectTabUseCase = object : TabsUseCases.SelectTabUseCase {
        override fun invoke(tabId: String) {
            interactor.onExistingSessionSelected(tabId)
        }
    }

    init {
        val components = activity.components
        val primaryTextColor = activity.getColorFromAttr(R.attr.textPrimary)

        val engineForSpeculativeConnects = when (activity.browsingModeManager.mode) {
            BrowsingMode.Normal -> components.core.engine
            BrowsingMode.Private -> null
        }
        sessionProvider =
            SessionSuggestionProvider(
                activity.resources,
                components.core.store,
                selectTabUseCase,
                components.core.icons,
                getDrawable(activity, R.drawable.ic_search_results_tab),
                excludeSelectedSession = !fromHomeFragment,
                suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            )

        historyStorageProvider =
            HistoryStorageSuggestionProvider(
                components.core.historyStorage,
                loadUrlUseCase,
                components.core.icons,
                engineForSpeculativeConnects,
                suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            )

        combinedHistoryProvider =
            CombinedHistorySuggestionProvider(
                historyStorage = components.core.historyStorage,
                historyMetadataStorage = components.core.historyStorage,
                loadUrlUseCase = loadUrlUseCase,
                icons = components.core.icons,
                engine = engineForSpeculativeConnects,
                maxNumberOfSuggestions = METADATA_SUGGESTION_LIMIT,
                suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            )

        bookmarksStorageSuggestionProvider =
            BookmarksStorageSuggestionProvider(
                bookmarksStorage = components.core.bookmarksStorage,
                loadUrlUseCase = loadUrlUseCase,
                icons = components.core.icons,
                indicatorIcon = getDrawable(activity, R.drawable.ic_search_results_bookmarks),
                engine = engineForSpeculativeConnects,
                suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            )

        syncedTabsStorageSuggestionProvider =
            SyncedTabsStorageSuggestionProvider(
                components.backgroundServices.syncedTabsStorage,
                loadUrlUseCase,
                components.core.icons,
                DeviceIndicators(
                    getDrawable(activity, R.drawable.ic_search_results_device_desktop),
                    getDrawable(activity, R.drawable.ic_search_results_device_mobile),
                    getDrawable(activity, R.drawable.ic_search_results_device_tablet),
                ),
                suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            )

        val searchBitmap = getDrawable(activity, R.drawable.ic_search)!!.apply {
            colorFilter = createBlendModeColorFilterCompat(primaryTextColor, SRC_IN)
        }.toBitmap()

        val searchWithBitmap = getDrawable(activity, R.drawable.ic_search_with)?.toBitmap()

        defaultSearchSuggestionProvider =
            SearchSuggestionProvider(
                context = activity,
                store = components.core.store,
                searchUseCase = searchUseCase,
                fetchClient = components.core.client,
                mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                limit = 3,
                icon = searchBitmap,
                showDescription = false,
                engine = engineForSpeculativeConnects,
                filterExactMatch = true,
                private = when (activity.browsingModeManager.mode) {
                    BrowsingMode.Normal -> false
                    BrowsingMode.Private -> true
                },
                suggestionsHeader = getSearchEngineSuggestionsHeader(),
            )

        defaultSearchActionProvider =
            SearchActionProvider(
                store = components.core.store,
                searchUseCase = searchUseCase,
                icon = searchBitmap,
                showDescription = false,
                suggestionsHeader = getSearchEngineSuggestionsHeader(),
            )

        shortcutsEnginePickerProvider =
            ShortcutsSuggestionProvider(
                store = components.core.store,
                context = activity,
                selectShortcutEngine = interactor::onSearchShortcutEngineSelected,
                selectShortcutEngineSettings = interactor::onClickSearchEngineSettings,
            )

        searchEngineSuggestionProvider =
            SearchEngineSuggestionProvider(
                context = activity,
                searchEnginesList = components.core.store.state.search.searchEngines,
                selectShortcutEngine = interactor::onSearchEngineSuggestionSelected,
                title = R.string.search_engine_suggestions_title,
                description = activity.getString(R.string.search_engine_suggestions_description),
                searchIcon = searchWithBitmap,
            )

        searchSuggestionProviderMap = HashMap()
    }

    private fun getSearchEngineSuggestionsHeader(): String? {
        val searchState = activity.components.core.store.state.search
        var searchEngine = searchState.selectedOrDefaultSearchEngine?.name

        if (!searchEngine.isNullOrEmpty()) {
            searchEngine = when (searchEngine) {
                GOOGLE_SEARCH_ENGINE_NAME -> activity.getString(
                    R.string.google_search_engine_suggestion_header,
                )
                else -> activity.getString(
                    R.string.other_default_search_engine_suggestion_header,
                    searchEngine,
                )
            }
        }

        return searchEngine
    }

    fun update(state: SearchFragmentState) {
        // Do not make suggestions based on user's current URL unless it's a search shortcut
        if (state.query.isNotEmpty() && state.query == state.url && !state.showSearchShortcuts) {
            return
        }

        view.onInputChanged(state.query)
    }

    fun updateSuggestionProvidersVisibility(
        state: SearchProviderState,
    ) {
        view.removeAllProviders()

        if (state.showSearchShortcuts) {
            handleDisplayShortcutsProviders()
            return
        }

        for (provider in getProvidersToAdd(state)) {
            view.addProviders(provider)
        }
    }

    @Suppress("ComplexMethod")
    private fun getProvidersToAdd(
        state: SearchProviderState,
    ): MutableSet<AwesomeBar.SuggestionProvider> {
        val providersToAdd = mutableSetOf<AwesomeBar.SuggestionProvider>()

        when (state.searchEngineSource) {
            is SearchEngineSource.History -> {
                combinedHistoryProvider.setMaxNumberOfSuggestions(METADATA_HISTORY_SUGGESTION_LIMIT)
                historyStorageProvider.setMaxNumberOfSuggestions(METADATA_HISTORY_SUGGESTION_LIMIT)
            }
            else -> {
                combinedHistoryProvider.setMaxNumberOfSuggestions(METADATA_SUGGESTION_LIMIT)
                historyStorageProvider.setMaxNumberOfSuggestions(METADATA_SUGGESTION_LIMIT)
            }
        }

        if (state.showHistorySuggestions) {
            if (activity.settings().historyMetadataUIFeature) {
                providersToAdd.add(combinedHistoryProvider)
            } else {
                providersToAdd.add(historyStorageProvider)
            }
        }

        if (state.showBookmarkSuggestions) {
            providersToAdd.add(bookmarksStorageSuggestionProvider)
        }

        if (state.showSearchSuggestions) {
            providersToAdd.addAll(getSelectedSearchSuggestionProvider(state))
        }

        if (state.showSyncedTabsSuggestions) {
            providersToAdd.add(syncedTabsStorageSuggestionProvider)
        }

        if (activity.browsingModeManager.mode == BrowsingMode.Normal && state.showSessionSuggestions) {
            providersToAdd.add(sessionProvider)
        }

        if (!activity.settings().showUnifiedSearchFeature) {
            providersToAdd.add(searchEngineSuggestionProvider)
        }

        return providersToAdd
    }

    private fun getSelectedSearchSuggestionProvider(state: SearchProviderState): List<AwesomeBar.SuggestionProvider> {
        return when (state.searchEngineSource) {
            is SearchEngineSource.Default -> listOf(
                defaultSearchActionProvider,
                defaultSearchSuggestionProvider,
            )
            is SearchEngineSource.Shortcut -> getSuggestionProviderForEngine(
                state.searchEngineSource.searchEngine,
            )
            is SearchEngineSource.History -> emptyList()
            is SearchEngineSource.Bookmarks -> emptyList()
            is SearchEngineSource.Tabs -> emptyList()
            is SearchEngineSource.None -> emptyList()
        }
    }

    private fun handleDisplayShortcutsProviders() {
        view.addProviders(shortcutsEnginePickerProvider)
    }

    private fun getSuggestionProviderForEngine(engine: SearchEngine): List<AwesomeBar.SuggestionProvider> {
        return searchSuggestionProviderMap.getOrPut(engine) {
            val components = activity.components
            val primaryTextColor = activity.getColorFromAttr(R.attr.textPrimary)

            val searchBitmap = getDrawable(activity, R.drawable.ic_search)!!.apply {
                colorFilter = createBlendModeColorFilterCompat(primaryTextColor, SRC_IN)
            }.toBitmap()

            val engineForSpeculativeConnects = when (activity.browsingModeManager.mode) {
                BrowsingMode.Normal -> components.core.engine
                BrowsingMode.Private -> null
            }

            listOf(
                SearchActionProvider(
                    searchEngine = engine,
                    store = components.core.store,
                    searchUseCase = shortcutSearchUseCase,
                    icon = searchBitmap,
                ),
                SearchSuggestionProvider(
                    engine,
                    shortcutSearchUseCase,
                    components.core.client,
                    limit = if (activity.settings().showUnifiedSearchFeature) {
                        METADATA_SHORTCUT_SUGGESTION_LIMIT
                    } else {
                        METADATA_SUGGESTION_LIMIT
                    },
                    mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
                    icon = searchBitmap,
                    engine = engineForSpeculativeConnects,
                    filterExactMatch = true,
                    private = when (activity.browsingModeManager.mode) {
                        BrowsingMode.Normal -> false
                        BrowsingMode.Private -> true
                    },
                ),
            )
        }
    }

    data class SearchProviderState(
        val showSearchShortcuts: Boolean,
        val showHistorySuggestions: Boolean,
        val showBookmarkSuggestions: Boolean,
        val showSearchSuggestions: Boolean,
        val showSyncedTabsSuggestions: Boolean,
        val showSessionSuggestions: Boolean,
        val searchEngineSource: SearchEngineSource,
    )

    companion object {
        // Maximum number of suggestions returned.
        const val METADATA_SUGGESTION_LIMIT = 3

        const val GOOGLE_SEARCH_ENGINE_NAME = "Google"
    }
}

fun SearchFragmentState.toSearchProviderState() = AwesomeBarView.SearchProviderState(
    showSearchShortcuts,
    showHistorySuggestions,
    showBookmarkSuggestions,
    showSearchSuggestions,
    showSyncedTabsSuggestions,
    showSessionSuggestions,
    searchEngineSource,
)
