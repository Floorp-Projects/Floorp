/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState

internal object SearchReducer {
    /**
     * [SearchAction] Reducer function for modifying [SearchState].
     */
    fun reduce(state: BrowserState, action: SearchAction): BrowserState {
        return when (action) {
            is SearchAction.AddSearchEngineListAction -> {
                state.addSearchEnginesMap(action.searchEngineList)
            }
            is SearchAction.SetCustomSearchEngineAction -> {
                state.setSearchEngine(action.searchEngine)
            }
            is SearchAction.RemoveCustomSearchEngineAction -> {
                state.removeSearchEngine(action.searchEngineId)
            }
            is SearchAction.SetDefaultSearchEngineAction -> {
                state.setDefaultSearchEngineAction(action.searchEngineId)
            }
        }
    }
}

private fun BrowserState.addSearchEnginesMap(
    searchEngineList: List<SearchEngine>
): BrowserState {
    return copy(search = search.copy(
        searchEngines = searchEngineList.map { it.id to it }.toMap()
    ))
}

private fun BrowserState.setSearchEngine(
    searchEngine: SearchEngine
): BrowserState {
    return copy(search = search.copy(
        searchEngines = search.searchEngines + (searchEngine.id to searchEngine)
    ))
}

private fun BrowserState.removeSearchEngine(
    searchEngineId: String
): BrowserState {
    return copy(search = search.copy(
        searchEngines = search.searchEngines - searchEngineId
    ))
}

private fun BrowserState.setDefaultSearchEngineAction(
    searchEngineId: String
): BrowserState {
    return if (search.searchEngines.containsKey(searchEngineId)) {
        copy(search = search.copy(
            defaultSearchEngineId = searchEngineId
        ))
    } else {
        this
    }
}
