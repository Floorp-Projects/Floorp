/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.utils.Settings

/**
 * The [Store] for holding the [SearchFragmentState] and applying [SearchFragmentAction]s.
 */
class SearchFragmentStore(
    initialState: SearchFragmentState
) : Store<SearchFragmentState, SearchFragmentAction>(
    initialState,
    ::searchStateReducer
)

/**
 * Wraps a `SearchEngine` to give consumers the context that it was selected as a shortcut
 */
sealed class SearchEngineSource {
    abstract val searchEngine: SearchEngine?

    /**
     * No search engine
     */
    object None : SearchEngineSource() {
        override val searchEngine: SearchEngine? = null
    }

    /**
     * Search engine set as default
     */
    data class Default(override val searchEngine: SearchEngine) : SearchEngineSource()

    /**
     * Search engine for quick search
     * This is for any search engine that is not the user selected default.
     */
    data class Shortcut(override val searchEngine: SearchEngine) : SearchEngineSource()

    /**
     * Search engine for history
     */
    data class History(override val searchEngine: SearchEngine) : SearchEngineSource()

    /**
     * Search engine for bookmarks
     */
    data class Bookmarks(override val searchEngine: SearchEngine) : SearchEngineSource()

    /**
     * Search engine for tabs
     */
    data class Tabs(override val searchEngine: SearchEngine) : SearchEngineSource()
}

/**
 * The state for the Search Screen
 *
 * @property query The current search query string
 * @property url The current URL of the tab (if this fragment is shown for an already existing tab)
 * @property searchTerms The search terms used to search previously in this tab (if this fragment is shown
 * for an already existing tab)
 * @property searchEngineSource The current selected search engine with the context of how it was selected
 * @property defaultEngine The current default search engine (or null if none is available yet)
 * @property showSearchSuggestions Whether or not to show search suggestions from the search engine in the AwesomeBar
 * @property showSearchSuggestionsHint Whether or not to show search suggestions in private hint panel
 * @property showSearchShortcuts Whether or not to show search shortcuts in the AwesomeBar
 * @property areShortcutsAvailable Whether or not there are >=2 search engines installed
 * so to know to present users with certain options or not.
 * @property showSearchShortcutsSetting Whether the setting for showing search shortcuts is enabled
 * or disabled.
 * @property showClipboardSuggestions Whether or not to show clipboard suggestion in the AwesomeBar
 * @property showHistorySuggestions Whether or not to show history suggestions in the AwesomeBar
 * @property showBookmarkSuggestions Whether or not to show the bookmark suggestion in the AwesomeBar
 * @property showSyncedTabsSuggestions Whether or not to show the synced tabs suggestion in the AwesomeBar
 * @property showSessionSuggestions Whether or not to show the session suggestion in the AwesomeBar
 * @property pastedText The text pasted from the long press toolbar menu
 * @property clipboardHasUrl Indicates if the clipboard contains an URL.
 */
data class SearchFragmentState(
    val query: String,
    val url: String,
    val searchTerms: String,
    val searchEngineSource: SearchEngineSource,
    val defaultEngine: SearchEngine?,
    val showSearchSuggestions: Boolean,
    val showSearchSuggestionsHint: Boolean,
    val showSearchShortcuts: Boolean,
    val areShortcutsAvailable: Boolean,
    val showSearchShortcutsSetting: Boolean,
    val showClipboardSuggestions: Boolean,
    val showHistorySuggestions: Boolean,
    val showBookmarkSuggestions: Boolean,
    val showSyncedTabsSuggestions: Boolean,
    val showSessionSuggestions: Boolean,
    val tabId: String?,
    val pastedText: String? = null,
    val searchAccessPoint: MetricsUtils.Source,
    val clipboardHasUrl: Boolean = false
) : State

/**
 * Creates the initial state for the search fragment.
 */
fun createInitialSearchFragmentState(
    activity: HomeActivity,
    components: Components,
    tabId: String?,
    pastedText: String?,
    searchAccessPoint: MetricsUtils.Source
): SearchFragmentState {
    val settings = components.settings
    val tab = tabId?.let { components.core.store.state.findTab(it) }
    val url = tab?.content?.url.orEmpty()

    val shouldShowSearchSuggestions = when (activity.browsingModeManager.mode) {
        BrowsingMode.Normal -> settings.shouldShowSearchSuggestions
        BrowsingMode.Private ->
            settings.shouldShowSearchSuggestions && settings.shouldShowSearchSuggestionsInPrivate
    }

    return SearchFragmentState(
        query = url,
        url = url,
        searchTerms = tab?.content?.searchTerms.orEmpty(),
        searchEngineSource = SearchEngineSource.None,
        defaultEngine = null,
        showSearchSuggestions = shouldShowSearchSuggestions,
        showSearchSuggestionsHint = false,
        showSearchShortcuts = false,
        areShortcutsAvailable = false,
        showSearchShortcutsSetting = settings.shouldShowSearchShortcuts,
        showClipboardSuggestions = settings.shouldShowClipboardSuggestions,
        showHistorySuggestions = settings.shouldShowHistorySuggestions,
        showBookmarkSuggestions = settings.shouldShowBookmarkSuggestions,
        showSyncedTabsSuggestions = settings.shouldShowSyncedTabsSuggestions,
        showSessionSuggestions = true,
        tabId = tabId,
        pastedText = pastedText,
        searchAccessPoint = searchAccessPoint
    )
}

/**
 * Actions to dispatch through the `SearchStore` to modify `SearchState` through the reducer.
 */
sealed class SearchFragmentAction : Action {
    /**
     * Action to enable or disable search suggestions.
     */
    data class SetShowSearchSuggestions(val show: Boolean) : SearchFragmentAction()

    /**
     * Action when default search engine is selected.
     */
    data class SearchDefaultEngineSelected(val engine: SearchEngine, val settings: Settings) : SearchFragmentAction()

    /**
     * Action when shortcut search engine is selected.
     */
    data class SearchShortcutEngineSelected(val engine: SearchEngine, val settings: Settings) : SearchFragmentAction()

    /**
     * Action when history search engine is selected.
     */
    data class SearchHistoryEngineSelected(val engine: SearchEngine) : SearchFragmentAction()

    /**
     * Action when bookmarks search engine is selected.
     */
    data class SearchBookmarksEngineSelected(val engine: SearchEngine) : SearchFragmentAction()

    /**
     * Action when tabs search engine is selected.
     */
    data class SearchTabsEngineSelected(val engine: SearchEngine) : SearchFragmentAction()

    /**
     * Action when search engine picker is selected.
     */
    data class ShowSearchShortcutEnginePicker(val show: Boolean) : SearchFragmentAction()

    /**
     * Action when allow search suggestion in private mode hint is tapped.
     */
    data class AllowSearchSuggestionsInPrivateModePrompt(val show: Boolean) : SearchFragmentAction()

    /**
     * Action when query is updated.
     */
    data class UpdateQuery(val query: String) : SearchFragmentAction()

    /**
     * Action when updating clipboard URL.
     */
    data class UpdateClipboardHasUrl(val hasUrl: Boolean) : SearchFragmentAction()

    /**
     * Updates the local `SearchFragmentState` from the global `SearchState` in `BrowserStore`.
     */
    data class UpdateSearchState(val search: SearchState) : SearchFragmentAction()
}

/**
 * The SearchState Reducer.
 */
@Suppress("LongMethod")
private fun searchStateReducer(state: SearchFragmentState, action: SearchFragmentAction): SearchFragmentState {
    return when (action) {
        is SearchFragmentAction.SearchDefaultEngineSelected ->
            state.copy(
                searchEngineSource = SearchEngineSource.Default(action.engine),
                showSearchSuggestions = true,
                showSearchShortcuts = action.settings.shouldShowSearchShortcuts,
                showClipboardSuggestions = action.settings.shouldShowClipboardSuggestions,
                showHistorySuggestions = action.settings.shouldShowHistorySuggestions,
                showBookmarkSuggestions = action.settings.shouldShowBookmarkSuggestions,
                showSyncedTabsSuggestions = action.settings.shouldShowSyncedTabsSuggestions,
                showSessionSuggestions = true,
            )
        is SearchFragmentAction.SearchShortcutEngineSelected ->
            state.copy(
                searchEngineSource = SearchEngineSource.Shortcut(action.engine),
                showSearchSuggestions = true,
                showSearchShortcuts = when (action.settings.showUnifiedSearchFeature) {
                    true -> false
                    false -> action.settings.shouldShowSearchShortcuts
                },
                showClipboardSuggestions = action.settings.shouldShowClipboardSuggestions,
                showHistorySuggestions = when (action.settings.showUnifiedSearchFeature) {
                    true -> false
                    false -> action.settings.shouldShowHistorySuggestions
                },
                showBookmarkSuggestions = when (action.settings.showUnifiedSearchFeature) {
                    true -> false
                    false -> action.settings.shouldShowBookmarkSuggestions
                },
                showSyncedTabsSuggestions = when (action.settings.showUnifiedSearchFeature) {
                    true -> false
                    false -> action.settings.shouldShowSyncedTabsSuggestions
                },
                showSessionSuggestions = when (action.settings.showUnifiedSearchFeature) {
                    true -> false
                    false -> true
                },
            )
        is SearchFragmentAction.SearchHistoryEngineSelected ->
            state.copy(
                searchEngineSource = SearchEngineSource.History(action.engine),
                showSearchSuggestions = false,
                showSearchShortcuts = false,
                showClipboardSuggestions = false,
                showHistorySuggestions = true,
                showBookmarkSuggestions = false,
                showSyncedTabsSuggestions = false,
                showSessionSuggestions = false,
            )
        is SearchFragmentAction.SearchBookmarksEngineSelected ->
            state.copy(
                searchEngineSource = SearchEngineSource.Bookmarks(action.engine),
                showSearchSuggestions = false,
                showSearchShortcuts = false,
                showClipboardSuggestions = false,
                showHistorySuggestions = false,
                showBookmarkSuggestions = true,
                showSyncedTabsSuggestions = false,
                showSessionSuggestions = false,
            )
        is SearchFragmentAction.SearchTabsEngineSelected ->
            state.copy(
                searchEngineSource = SearchEngineSource.Tabs(action.engine),
                showSearchSuggestions = false,
                showSearchShortcuts = false,
                showClipboardSuggestions = false,
                showHistorySuggestions = false,
                showBookmarkSuggestions = false,
                showSyncedTabsSuggestions = true,
                showSessionSuggestions = true,
            )
        is SearchFragmentAction.ShowSearchShortcutEnginePicker ->
            state.copy(showSearchShortcuts = action.show && state.areShortcutsAvailable)
        is SearchFragmentAction.UpdateQuery ->
            state.copy(query = action.query)
        is SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt ->
            state.copy(showSearchSuggestionsHint = action.show)
        is SearchFragmentAction.SetShowSearchSuggestions ->
            state.copy(showSearchSuggestions = action.show)
        is SearchFragmentAction.UpdateSearchState -> {
            state.copy(
                defaultEngine = action.search.selectedOrDefaultSearchEngine,
                areShortcutsAvailable = action.search.searchEngines.size > 1,
                showSearchShortcuts = state.url.isEmpty() &&
                    state.showSearchShortcutsSetting &&
                    action.search.searchEngines.size > 1,
                searchEngineSource = when (state.searchEngineSource) {
                    is SearchEngineSource.Default, is SearchEngineSource.None -> {
                        action.search.selectedOrDefaultSearchEngine?.let { SearchEngineSource.Default(it) }
                            ?: SearchEngineSource.None
                    }
                    else -> {
                        state.searchEngineSource
                    }
                }
            )
        }
        is SearchFragmentAction.UpdateClipboardHasUrl -> {
            state.copy(
                clipboardHasUrl = action.hasUrl
            )
        }
    }
}
