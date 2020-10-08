/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SearchState

internal object SearchReducer {
    /**
     * [SearchAction] Reducer function for modifying [SearchState].
     */
    fun reduce(state: BrowserState, action: SearchAction): BrowserState {
        return when (action) {
            is SearchAction.SetRegionAction -> state.setRegion(action)
            is SearchAction.SetRegionSearchEngines -> state.setRegionSearchEngines(action)
            is SearchAction.SetCustomSearchEngines -> state.setCustomSearchEngines(action)
            is SearchAction.UpdateCustomSearchEngineAction -> state.updateCustomSearchEngine(action)
            is SearchAction.RemoveCustomSearchEngineAction -> state.removeSearchEngine(action)
            is SearchAction.SetDefaultSearchEngineAction -> state.setDefaultSearchEngineAction(action)
        }
    }
}

private fun BrowserState.setRegionSearchEngines(
    action: SearchAction.SetRegionSearchEngines
): BrowserState {
    return copy(search = search.copy(
        regionSearchEngines = action.searchEngines,
        regionDefaultSearchEngineId = action.regionDefaultSearchEngineId
    ))
}

private fun BrowserState.setCustomSearchEngines(
    action: SearchAction.SetCustomSearchEngines
): BrowserState {
    return copy(search = search.copy(
        customSearchEngines = action.searchEngines
    ))
}

private fun BrowserState.setRegion(
    action: SearchAction.SetRegionAction
): BrowserState {
    return copy(search = search.copy(
        region = action.regionState
    ))
}

private fun BrowserState.updateCustomSearchEngine(
    action: SearchAction.UpdateCustomSearchEngineAction
): BrowserState {
    var updated = false
    val searchEngines = search.customSearchEngines.map {
        if (it.id == action.searchEngine.id) {
            updated = true
            action.searchEngine
        } else {
            it
        }
    }

    return copy(search = search.copy(
        customSearchEngines = if (updated) {
            searchEngines
        } else {
            searchEngines + action.searchEngine
        }
    ))
}

private fun BrowserState.removeSearchEngine(
    action: SearchAction.RemoveCustomSearchEngineAction
): BrowserState {
    return copy(search = search.copy(
        customSearchEngines = search.customSearchEngines.filter { it.id != action.searchEngineId }
    ))
}

private fun BrowserState.setDefaultSearchEngineAction(
    action: SearchAction.SetDefaultSearchEngineAction
): BrowserState {
    // We allow setting an ID of a search engine that is not in the state since loading the search
    // engines may happen asynchronously and the search engine may not be loaded yet at this point.
    return copy(search = search.copy(
        defaultSearchEngineId = action.searchEngineId
    ))
}
