/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.utils.Settings

class TrackingProtectionStore(
    val initialState: TrackingProtectionState
) : Store<TrackingProtectionState, TrackingProtectionAction>(
    initialState, ::trackingProtectionReducer
) {
    companion object {
        fun createStore(
            url: String,
            isTrackingProtectionOn: Boolean,
            settings: Settings
        ) = TrackingProtectionStore(
            TrackingProtectionState(
                websiteUrl = url,
                isTrackingProtectionEnabledPerApp = settings.shouldUseTrackingProtection,
                isTrackingProtectionEnabledPerWebsite = isTrackingProtectionOn
            )
        )
    }
}

data class TrackingProtectionState(
    val websiteUrl: String,
    val isTrackingProtectionEnabledPerApp: Boolean,
    val isTrackingProtectionEnabledPerWebsite: Boolean
) : State

sealed class TrackingProtectionAction : Action {
    object Stub1 : TrackingProtectionAction()
    object Stub2 : TrackingProtectionAction()
}

fun trackingProtectionReducer(
    state: TrackingProtectionState,
    action: TrackingProtectionAction
): TrackingProtectionState {
    return when (action) {
        TrackingProtectionAction.Stub1 -> state
        TrackingProtectionAction.Stub2 -> state
    }
}
