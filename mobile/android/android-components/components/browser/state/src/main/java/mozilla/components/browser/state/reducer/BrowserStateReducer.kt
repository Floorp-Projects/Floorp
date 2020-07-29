/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContainerAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.action.ReaderAction
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.action.WebExtensionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.Action

/**
 * Reducers for [BrowserStore].
 *
 * A reducer is a function that receives the current [BrowserState] and an [Action] and then returns a new
 * [BrowserState].
 */
internal object BrowserStateReducer {
    fun reduce(state: BrowserState, action: BrowserAction): BrowserState {
        return when (action) {
            is ContainerAction -> ContainerReducer.reduce(state, action)
            is ContentAction -> ContentStateReducer.reduce(state, action)
            is CustomTabListAction -> CustomTabListReducer.reduce(state, action)
            is EngineAction -> EngineStateReducer.reduce(state, action)
            is ReaderAction -> ReaderStateReducer.reduce(state, action)
            is SystemAction -> SystemReducer.reduce(state, action)
            is TabListAction -> TabListReducer.reduce(state, action)
            is TrackingProtectionAction -> TrackingProtectionStateReducer.reduce(state, action)
            is WebExtensionAction -> WebExtensionReducer.reduce(state, action)
            is MediaAction -> MediaReducer.reduce(state, action)
            is DownloadAction -> DownloadStateReducer.reduce(state, action)
            is SearchAction -> SearchReducer.reduce(state, action)
        }
    }
}

/**
 * Finds the corresponding tab in the [BrowserState] and replaces it using [update].
 * @param tabId ID of the tab to change.
 * @param update Returns a new version of the tab state. Must be the same class,
 * preferably using [SessionState.createCopy].
 */
@Suppress("Unchecked_Cast")
internal fun BrowserState.updateTabState(
    tabId: String,
    update: (SessionState) -> SessionState
): BrowserState {
    val newTabs = tabs.updateTabs(tabId, update) as List<TabSessionState>?
    if (newTabs != null) return copy(tabs = newTabs)

    val newCustomTabs = customTabs.updateTabs(tabId, update) as List<CustomTabSessionState>?
    if (newCustomTabs != null) return copy(customTabs = newCustomTabs)

    return this
}

/**
 * Finds the corresponding tab in the list and replaces it using [update].
 * @param tabId ID of the tab to change.
 * @param update Returns a new version of the tab state.
 */
internal fun <T : SessionState> List<T>.updateTabs(
    tabId: String,
    update: (T) -> T
): List<T>? {
    val tabIndex = indexOfFirst { it.id == tabId }
    if (tabIndex == -1) return null

    return subList(0, tabIndex) + update(get(tabIndex)) + subList(tabIndex + 1, size)
}
