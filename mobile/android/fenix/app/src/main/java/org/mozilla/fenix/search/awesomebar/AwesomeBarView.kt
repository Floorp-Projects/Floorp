/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.awesomebar

import android.content.Context
import android.graphics.drawable.Drawable
import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.graphics.BlendModeColorFilterCompat.createBlendModeColorFilterCompat
import androidx.core.graphics.BlendModeCompat.SRC_IN
import androidx.core.graphics.drawable.toBitmap
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.CombinedHistorySuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchActionProvider
import mozilla.components.feature.awesomebar.provider.SearchEngineSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchTermSuggestionsProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.fxsuggest.FxSuggestSuggestionProvider
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.syncedtabs.DeviceIndicators
import mozilla.components.feature.syncedtabs.SyncedTabsStorageSuggestionProvider
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.android.net.sameHostWithoutMobileSubdomainAs
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import mozilla.components.support.ktx.kotlin.urlContainsQueryParameters
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.Core.Companion.METADATA_HISTORY_SUGGESTION_LIMIT
import org.mozilla.fenix.components.Core.Companion.METADATA_SHORTCUT_SUGGESTION_LIMIT
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.containsQueryParameters
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
    private val fromHomeFragment: Boolean,
) {
    private var components: Components = activity.components
    private val engineForSpeculativeConnects: Engine?
    private val defaultHistoryStorageProvider: HistoryStorageSuggestionProvider
    private val defaultCombinedHistoryProvider: CombinedHistorySuggestionProvider
    private val shortcutsEnginePickerProvider: ShortcutsSuggestionProvider
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

    private val historySearchTermUseCase = object : SearchUseCases.SearchUseCase {
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
        val primaryTextColor = activity.getColorFromAttr(R.attr.textPrimary)

        engineForSpeculativeConnects = when (activity.browsingModeManager.mode) {
            BrowsingMode.Normal -> components.core.engine
            BrowsingMode.Private -> null
        }

        defaultHistoryStorageProvider =
            HistoryStorageSuggestionProvider(
                components.core.historyStorage,
                loadUrlUseCase,
                components.core.icons,
                engineForSpeculativeConnects,
                showEditSuggestion = false,
                suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            )

        defaultCombinedHistoryProvider =
            CombinedHistorySuggestionProvider(
                historyStorage = components.core.historyStorage,
                historyMetadataStorage = components.core.historyStorage,
                loadUrlUseCase = loadUrlUseCase,
                icons = components.core.icons,
                engine = engineForSpeculativeConnects,
                maxNumberOfSuggestions = METADATA_SUGGESTION_LIMIT,
                showEditSuggestion = false,
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
                GOOGLE_SEARCH_ENGINE_NAME -> getString(
                    activity,
                    R.string.google_search_engine_suggestion_header,
                )
                else -> getString(
                    activity,
                    R.string.other_default_search_engine_suggestion_header,
                    searchEngine,
                )
            }
        }

        return searchEngine
    }

    /**
     * Get a suggestions header if [currentEngineName] is the one of the default search engine.
     *
     * @param currentEngine The currently selected search engine.
     */
    private fun getSearchEngineSuggestionsHeader(currentEngine: SearchEngine?): String? {
        val defaultSearchEngine = activity.components.core.store.state.search.selectedOrDefaultSearchEngine

        return when (defaultSearchEngine == currentEngine) {
            true -> getSearchEngineSuggestionsHeader()
            false -> null
        }
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

    @Suppress("ComplexMethod", "LongMethod")
    @VisibleForTesting
    internal fun getProvidersToAdd(
        state: SearchProviderState,
    ): MutableSet<AwesomeBar.SuggestionProvider> {
        val providersToAdd = mutableSetOf<AwesomeBar.SuggestionProvider>()

        when (state.searchEngineSource) {
            is SearchEngineSource.History -> {
                defaultCombinedHistoryProvider.setMaxNumberOfSuggestions(METADATA_HISTORY_SUGGESTION_LIMIT)
                defaultHistoryStorageProvider.setMaxNumberOfSuggestions(METADATA_HISTORY_SUGGESTION_LIMIT)
            }
            else -> {
                defaultCombinedHistoryProvider.setMaxNumberOfSuggestions(METADATA_SUGGESTION_LIMIT)
                defaultHistoryStorageProvider.setMaxNumberOfSuggestions(METADATA_SUGGESTION_LIMIT)
            }
        }

        if (state.showSearchTermHistory) {
            getSearchTermSuggestionsProvider(state.searchEngineSource)?.let {
                providersToAdd.add(it)
            }
        }

        if (state.showAllHistorySuggestions) {
            providersToAdd.add(
                getHistoryProvider(
                    filter = getFilterToExcludeSponsoredResults(state),
                ),
            )
        }

        if (state.showHistorySuggestionsForCurrentEngine) {
            getFilterForCurrentEngineResults(state)?.let {
                providersToAdd.add(getHistoryProvider(it))
            }
        }

        if (state.showAllBookmarkSuggestions) {
            providersToAdd.add(
                getBookmarksProvider(
                    filter = getFilterToExcludeSponsoredResults(state),
                ),
            )
        }

        if (state.showBookmarksSuggestionsForCurrentEngine) {
            getFilterForCurrentEngineResults(state)?.let {
                providersToAdd.add(getBookmarksProvider(it))
            }
        }

        if (state.showSearchSuggestions) {
            providersToAdd.addAll(getSelectedSearchSuggestionProvider(state))
        }

        if (state.showAllSyncedTabsSuggestions) {
            providersToAdd.add(
                getSyncedTabsProvider(
                    filter = getFilterToExcludeSponsoredResults(state),
                ),
            )
        }

        if (state.showSyncedTabsSuggestionsForCurrentEngine) {
            getFilterForCurrentEngineResults(state)?.let {
                providersToAdd.add(getSyncedTabsProvider(it))
            }
        }

        if (activity.browsingModeManager.mode == BrowsingMode.Normal && state.showAllSessionSuggestions) {
            // Unlike other providers, we don't exclude sponsored suggestions for open tabs.
            providersToAdd.add(getLocalTabsProvider())
        }

        if (activity.browsingModeManager.mode == BrowsingMode.Normal && state.showSessionSuggestionsForCurrentEngine) {
            getFilterForCurrentEngineResults(state)?.let {
                providersToAdd.add(getLocalTabsProvider(it))
            }
        }

        if (state.showSponsoredSuggestions || state.showNonSponsoredSuggestions) {
            providersToAdd.add(
                FxSuggestSuggestionProvider(
                    resources = activity.resources,
                    loadUrlUseCase = loadUrlUseCase,
                    includeSponsoredSuggestions = state.showSponsoredSuggestions,
                    includeNonSponsoredSuggestions = state.showNonSponsoredSuggestions,
                    suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
                    contextId = activity.settings().contileContextId,
                ),
            )
        }

        providersToAdd.add(searchEngineSuggestionProvider)

        return providersToAdd
    }

    /**
     * Get a history suggestion provider configured for this [AwesomeBarView].
     *
     * @param filter Optional filter to limit the returned history suggestions.
     *
     * @return A [CombinedHistorySuggestionProvider] or [HistoryStorageSuggestionProvider] depending
     * on if the history metadata feature is enabled.
     */
    @VisibleForTesting
    internal fun getHistoryProvider(
        filter: SearchResultFilter? = null,
    ): AwesomeBar.SuggestionProvider {
        return if (activity.settings().historyMetadataUIFeature) {
            if (filter != null) {
                CombinedHistorySuggestionProvider(
                    historyStorage = components.core.historyStorage,
                    historyMetadataStorage = components.core.historyStorage,
                    loadUrlUseCase = loadUrlUseCase,
                    icons = components.core.icons,
                    engine = engineForSpeculativeConnects,
                    maxNumberOfSuggestions = METADATA_SUGGESTION_LIMIT,
                    showEditSuggestion = false,
                    suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
                    resultsUriFilter = filter::shouldIncludeUri,
                )
            } else {
                defaultCombinedHistoryProvider
            }
        } else {
            if (filter != null) {
                HistoryStorageSuggestionProvider(
                    historyStorage = components.core.historyStorage,
                    loadUrlUseCase = loadUrlUseCase,
                    icons = components.core.icons,
                    engine = engineForSpeculativeConnects,
                    maxNumberOfSuggestions = METADATA_SUGGESTION_LIMIT,
                    showEditSuggestion = false,
                    suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
                    resultsUriFilter = filter::shouldIncludeUri,
                )
            } else {
                defaultHistoryStorageProvider
            }
        }
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

    @VisibleForTesting
    internal fun getSearchTermSuggestionsProvider(
        searchEngineSource: SearchEngineSource,
    ): AwesomeBar.SuggestionProvider? {
        val validSearchEngine = searchEngineSource.searchEngine ?: return null

        return SearchTermSuggestionsProvider(
            historyStorage = components.core.historyStorage,
            searchUseCase = historySearchTermUseCase,
            searchEngine = validSearchEngine,
            icon = getDrawable(activity, R.drawable.ic_history)?.toBitmap(),
            engine = engineForSpeculativeConnects,
            suggestionsHeader = getSearchEngineSuggestionsHeader(searchEngineSource.searchEngine),
        )
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
                    limit = METADATA_SHORTCUT_SUGGESTION_LIMIT,
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

    /**
     * Get a synced tabs provider configured for this [AwesomeBarView].
     *
     * @param filter Optional filter to limit the returned synced tab suggestions.
     *
     * @return [SyncedTabsStorageSuggestionProvider] providing suggestions for the [AwesomeBar].
     */
    @VisibleForTesting
    internal fun getSyncedTabsProvider(
        filter: SearchResultFilter? = null,
    ): SyncedTabsStorageSuggestionProvider {
        return SyncedTabsStorageSuggestionProvider(
            components.backgroundServices.syncedTabsStorage,
            loadUrlUseCase,
            components.core.icons,
            DeviceIndicators(
                getDrawable(activity, R.drawable.ic_search_results_device_desktop),
                getDrawable(activity, R.drawable.ic_search_results_device_mobile),
                getDrawable(activity, R.drawable.ic_search_results_device_tablet),
            ),
            suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            resultsUrlFilter = filter?.let { it::shouldIncludeUrl },
        )
    }

    /**
     * Get a local tabs provider configured for this [AwesomeBarView].
     *
     * @param filter Optional filter to limit the returned local tab suggestions.
     *
     * @return [SessionSuggestionProvider] providing suggestions for the [AwesomeBar].
     */
    @VisibleForTesting
    internal fun getLocalTabsProvider(
        filter: SearchResultFilter? = null,
    ): SessionSuggestionProvider {
        return SessionSuggestionProvider(
            activity.resources,
            components.core.store,
            selectTabUseCase,
            components.core.icons,
            getDrawable(activity, R.drawable.ic_search_results_tab),
            excludeSelectedSession = !fromHomeFragment,
            suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            resultsUriFilter = filter?.let { it::shouldIncludeUri },
        )
    }

    /**
     * Get a bookmarks provider configured for this [AwesomeBarView].
     *
     * @param filter Optional filter to limit the returned bookmark suggestions.
     *
     * @return [BookmarksStorageSuggestionProvider] providing suggestions for the [AwesomeBar].
     */
    @VisibleForTesting
    internal fun getBookmarksProvider(
        filter: SearchResultFilter? = null,
    ): BookmarksStorageSuggestionProvider {
        return BookmarksStorageSuggestionProvider(
            bookmarksStorage = components.core.bookmarksStorage,
            loadUrlUseCase = loadUrlUseCase,
            icons = components.core.icons,
            indicatorIcon = getDrawable(activity, R.drawable.ic_search_results_bookmarks),
            engine = engineForSpeculativeConnects,
            showEditSuggestion = false,
            suggestionsHeader = activity.getString(R.string.firefox_suggest_header),
            resultsUriFilter = filter?.let { it::shouldIncludeUri },
        )
    }

    /**
     * Returns a [SearchResultFilter] that only includes results for the current search engine.
     */
    internal fun getFilterForCurrentEngineResults(state: SearchProviderState): SearchResultFilter? =
        state.searchEngineSource.searchEngine?.resultsUrl?.let {
            SearchResultFilter.CurrentEngine(it)
        }

    /**
     * Returns a [SearchResultFilter] that excludes sponsored results.
     */
    internal fun getFilterToExcludeSponsoredResults(state: SearchProviderState): SearchResultFilter? =
        if (state.showSponsoredSuggestions) {
            SearchResultFilter.ExcludeSponsored(activity.settings().frecencyFilterQuery)
        } else {
            null
        }

    data class SearchProviderState(
        val showSearchShortcuts: Boolean,
        val showSearchTermHistory: Boolean,
        val showHistorySuggestionsForCurrentEngine: Boolean,
        val showAllHistorySuggestions: Boolean,
        val showBookmarksSuggestionsForCurrentEngine: Boolean,
        val showAllBookmarkSuggestions: Boolean,
        val showSearchSuggestions: Boolean,
        val showSyncedTabsSuggestionsForCurrentEngine: Boolean,
        val showAllSyncedTabsSuggestions: Boolean,
        val showSessionSuggestionsForCurrentEngine: Boolean,
        val showAllSessionSuggestions: Boolean,
        val showSponsoredSuggestions: Boolean,
        val showNonSponsoredSuggestions: Boolean,
        val searchEngineSource: SearchEngineSource,
    )

    /**
     * Filters to limit the suggestions returned from a suggestion provider.
     */
    sealed interface SearchResultFilter {
        /**
         * A filter for the currently selected search engine. This filter only includes suggestions
         * whose URLs have the same host as [resultsUri].
         */
        data class CurrentEngine(val resultsUri: Uri) : SearchResultFilter

        /**
         * A filter that excludes sponsored suggestions, whose URLs contain the given
         * [queryParameter].
         */
        data class ExcludeSponsored(val queryParameter: String) : SearchResultFilter

        /**
         * Returns `true` if the suggestion with the given [uri] should be included in the
         * suggestions returned from the provider.
         */
        fun shouldIncludeUri(uri: Uri): Boolean = when (this) {
            is CurrentEngine -> this.resultsUri.sameHostWithoutMobileSubdomainAs(uri)
            is ExcludeSponsored -> !uri.containsQueryParameters(queryParameter)
        }

        /**
         * Returns `true` if the suggestion with the given [url] string should be included in the
         * suggestions returned from the provider.
         */
        fun shouldIncludeUrl(url: String): Boolean = when (this) {
            is CurrentEngine -> resultsUri.host == url.tryGetHostFromUrl()
            is ExcludeSponsored -> !url.urlContainsQueryParameters(queryParameter)
        }
    }

    companion object {
        // Maximum number of suggestions returned.
        const val METADATA_SUGGESTION_LIMIT = 3

        const val GOOGLE_SEARCH_ENGINE_NAME = "Google"

        @VisibleForTesting
        internal fun getDrawable(context: Context, resId: Int): Drawable? {
            return AppCompatResources.getDrawable(context, resId)
        }

        @VisibleForTesting
        internal fun getString(context: Context, resId: Int, vararg formatArgs: String?): String {
            return context.getString(resId, *formatArgs)
        }
    }
}

fun SearchFragmentState.toSearchProviderState() = AwesomeBarView.SearchProviderState(
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
