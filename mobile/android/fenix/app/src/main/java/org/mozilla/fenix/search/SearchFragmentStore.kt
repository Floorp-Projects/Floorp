/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.state.selector.findTab
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.search.ext.areShortcutsAvailable

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
    abstract val searchEngine: SearchEngine

    data class Default(override val searchEngine: SearchEngine) : SearchEngineSource()
    data class Shortcut(override val searchEngine: SearchEngine) : SearchEngineSource()
}

/**
 * The state for the Search Screen
 * @property query The current search query string
 * @property url The current URL of the tab (if this fragment is shown for an already existing tab)
 * @property searchTerms The search terms used to search previously in this tab (if this fragment is shown
 * for an already existing tab)
 * @property searchEngineSource The current selected search engine with the context of how it was selected
 * @property defaultEngineSource The current default search engine source
 * @property showSearchSuggestions Whether or not to show search suggestions from the search engine in the AwesomeBar
 * @property showSearchSuggestionsHint Whether or not to show search suggestions in private hint panel
 * @property showSearchShortcuts Whether or not to show search shortcuts in the AwesomeBar
 * @property areShortcutsAvailable Whether or not there are >=2 search engines installed
 *                                 so to know to present users with certain options or not.
 * @property showClipboardSuggestions Whether or not to show clipboard suggestion in the AwesomeBar
 * @property showHistorySuggestions Whether or not to show history suggestions in the AwesomeBar
 * @property showBookmarkSuggestions Whether or not to show the bookmark suggestion in the AwesomeBar
 * @property pastedText The text pasted from the long press toolbar menu
 */
data class SearchFragmentState(
    val query: String,
    val url: String,
    val searchTerms: String,
    val searchEngineSource: SearchEngineSource,
    val defaultEngineSource: SearchEngineSource.Default,
    val showSearchSuggestions: Boolean,
    val showSearchSuggestionsHint: Boolean,
    val showSearchShortcuts: Boolean,
    val areShortcutsAvailable: Boolean,
    val showClipboardSuggestions: Boolean,
    val showHistorySuggestions: Boolean,
    val showBookmarkSuggestions: Boolean,
    val tabId: String?,
    val pastedText: String? = null,
    val searchAccessPoint: Event.PerformedSearch.SearchAccessPoint?
) : State

fun createInitialSearchFragmentState(
    activity: HomeActivity,
    components: Components,
    tabId: String?,
    pastedText: String?,
    searchAccessPoint: Event.PerformedSearch.SearchAccessPoint
): SearchFragmentState {
    val settings = components.settings
    val tab = tabId?.let { components.core.store.state.findTab(it) }

    val url = tab?.content?.url.orEmpty()
    val currentSearchEngine = SearchEngineSource.Default(
        components.search.provider.getDefaultEngine(activity)
    )

    val browsingMode = activity.browsingModeManager.mode
    val areShortcutsAvailable = components.search.provider.areShortcutsAvailable(activity)

    val shouldShowSearchSuggestions = when (browsingMode) {
        BrowsingMode.Normal -> settings.shouldShowSearchSuggestions
        BrowsingMode.Private ->
            settings.shouldShowSearchSuggestions && settings.shouldShowSearchSuggestionsInPrivate
    }

    return SearchFragmentState(
        query = url,
        url = url,
        searchTerms = tab?.content?.searchTerms.orEmpty(),
        searchEngineSource = currentSearchEngine,
        defaultEngineSource = currentSearchEngine,
        showSearchSuggestions = shouldShowSearchSuggestions,
        showSearchSuggestionsHint = false,
        showSearchShortcuts = url.isEmpty() &&
            areShortcutsAvailable &&
            settings.shouldShowSearchShortcuts,
        areShortcutsAvailable = areShortcutsAvailable,
        showClipboardSuggestions = settings.shouldShowClipboardSuggestions,
        showHistorySuggestions = settings.shouldShowHistorySuggestions,
        showBookmarkSuggestions = settings.shouldShowBookmarkSuggestions,
        tabId = tabId,
        pastedText = pastedText,
        searchAccessPoint = searchAccessPoint
    )
}

/**
 * Actions to dispatch through the `SearchStore` to modify `SearchState` through the reducer.
 */
sealed class SearchFragmentAction : Action {
    data class SetShowSearchSuggestions(val show: Boolean) : SearchFragmentAction()
    data class SearchShortcutEngineSelected(val engine: SearchEngine) : SearchFragmentAction()
    data class SelectNewDefaultSearchEngine(val engine: SearchEngine) : SearchFragmentAction()
    data class ShowSearchShortcutEnginePicker(val show: Boolean) : SearchFragmentAction()
    data class UpdateShortcutsAvailability(val areShortcutsAvailable: Boolean) : SearchFragmentAction()
    data class AllowSearchSuggestionsInPrivateModePrompt(val show: Boolean) : SearchFragmentAction()
    data class UpdateQuery(val query: String) : SearchFragmentAction()
}

/**
 * The SearchState Reducer.
 */
private fun searchStateReducer(state: SearchFragmentState, action: SearchFragmentAction): SearchFragmentState {
    return when (action) {
        is SearchFragmentAction.SearchShortcutEngineSelected ->
            state.copy(
                searchEngineSource = SearchEngineSource.Shortcut(action.engine),
                showSearchShortcuts = false
            )
        is SearchFragmentAction.ShowSearchShortcutEnginePicker ->
            state.copy(showSearchShortcuts = action.show && state.areShortcutsAvailable)
        is SearchFragmentAction.UpdateShortcutsAvailability ->
            state.copy(areShortcutsAvailable = action.areShortcutsAvailable)
        is SearchFragmentAction.UpdateQuery ->
            state.copy(query = action.query)
        is SearchFragmentAction.SelectNewDefaultSearchEngine ->
            state.copy(searchEngineSource = SearchEngineSource.Default(action.engine))
        is SearchFragmentAction.AllowSearchSuggestionsInPrivateModePrompt ->
            state.copy(showSearchSuggestionsHint = action.show)
        is SearchFragmentAction.SetShowSearchSuggestions ->
            state.copy(showSearchSuggestions = action.show)
    }
}
