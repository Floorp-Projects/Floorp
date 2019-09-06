/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TrackingProtectionState

internal object TrackingProtectionStateReducer {
    /**
     * [TrackingProtectionAction] Reducer function for modifying a specific [TrackingProtectionState]
     * of a [SessionState].
     */
    fun reduce(state: BrowserState, action: TrackingProtectionAction): BrowserState = when (action) {
        is TrackingProtectionAction.ToggleAction -> state.copyWithTrackingProtectionState(action.tabId) {
            it.copy(enabled = action.enabled)
        }
        is TrackingProtectionAction.TrackerBlockedAction -> state.copyWithTrackingProtectionState(action.tabId) {
            it.copy(blockedTrackers = it.blockedTrackers + action.tracker)
        }
        is TrackingProtectionAction.TrackerLoadedAction -> state.copyWithTrackingProtectionState(action.tabId) {
            it.copy(loadedTrackers = it.loadedTrackers + action.tracker)
        }
        is TrackingProtectionAction.ClearTrackersAction -> state.copyWithTrackingProtectionState(action.tabId) {
            it.copy(loadedTrackers = emptyList(), blockedTrackers = emptyList())
        }
    }
}

private fun BrowserState.copyWithTrackingProtectionState(
    tabId: String,
    update: (TrackingProtectionState) -> TrackingProtectionState
): BrowserState {
    // Currently we map over both lists (tabs and customTabs). We could optimize this away later on if we know what
    // type we want to modify.
    return copy(
        tabs = tabs.map { current ->
            if (current.id == tabId) {
                current.copy(trackingProtection = update.invoke(current.trackingProtection))
            } else {
                current
            }
        },
        customTabs = customTabs.map { current ->
            if (current.id == tabId) {
                current.copy(trackingProtection = update.invoke(current.trackingProtection))
            } else {
                current
            }
        }
    )
}
