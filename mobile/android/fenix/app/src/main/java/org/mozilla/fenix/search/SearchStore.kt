/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.session.Session
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [SearchState] and applying [SearchAction]s.
 */
class SearchStore(
    initialState: SearchState
) : Store<SearchState, SearchAction>(
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
 * @property showShortcutEnginePicker Whether or not to show the available search engine view
 * @property searchEngineSource The current selected search engine with the context of how it was selected
 * @property defaultEngineSource The current default search engine source
 * @property showSuggestions Whether or not to show search suggestions for the selected search engine in the AwesomeBar
 * @property showVisitedSitesBookmarks Whether or not to show history and bookmark suggestions in the AwesomeBar
 * @property session The current session if available
 */
data class SearchState(
    val query: String,
    val showShortcutEnginePicker: Boolean,
    val searchEngineSource: SearchEngineSource,
    val defaultEngineSource: SearchEngineSource.Default,
    val showSuggestions: Boolean,
    val showVisitedSitesBookmarks: Boolean,
    val session: Session?
) : State

/**
 * Actions to dispatch through the `SearchStore` to modify `SearchState` through the reducer.
 */
sealed class SearchAction : Action {
    data class SearchShortcutEngineSelected(val engine: SearchEngine) : SearchAction()
    data class SelectNewDefaultSearchEngine(val engine: SearchEngine) : SearchAction()
    data class ShowSearchShortcutEnginePicker(val show: Boolean) : SearchAction()
    data class UpdateQuery(val query: String) : SearchAction()
}

/**
 * The SearchState Reducer.
 */
fun searchStateReducer(state: SearchState, action: SearchAction): SearchState {
    return when (action) {
        is SearchAction.SearchShortcutEngineSelected ->
            state.copy(
                searchEngineSource = SearchEngineSource.Shortcut(action.engine),
                showShortcutEnginePicker = false
            )
        is SearchAction.ShowSearchShortcutEnginePicker ->
            state.copy(showShortcutEnginePicker = action.show)
        is SearchAction.UpdateQuery ->
            state.copy(query = action.query)
        is SearchAction.SelectNewDefaultSearchEngine ->
            state.copy(
                searchEngineSource = SearchEngineSource.Default(action.engine),
                showShortcutEnginePicker = false
            )
    }
}
