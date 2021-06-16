/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import org.mozilla.fenix.trackingprotection.TrackingProtectionState

/**
 * Parent Reducer for all [QuickSettingsFragmentState]s of all Views shown in this Fragment.
 */
internal fun quickSettingsFragmentReducer(
    state: QuickSettingsFragmentState,
    action: QuickSettingsFragmentAction
): QuickSettingsFragmentState {
    return when (action) {
        is WebsiteInfoAction -> {
            // There is no possible action that can change this View's state while it is displayed to the user.
            // Every time the View is recreated it starts with a fresh state. This is the only way to display
            // something different.
            state
        }
        is WebsitePermissionAction -> state.copy(
            websitePermissionsState = WebsitePermissionsStateReducer.reduce(
                state.websitePermissionsState,
                action
            )
        )
        is TrackingProtectionAction -> state.copy(
            trackingProtectionState = TrackingProtectionStateReducer.reduce(
                state = state.trackingProtectionState,
                action = action
            )
        )
    }
}

object WebsitePermissionsStateReducer {
    /**
     * Handles creating a new [WebsitePermissionsState] based on the specific [WebsitePermissionAction]
     */
    fun reduce(
        state: WebsitePermissionsState,
        action: WebsitePermissionAction
    ): WebsitePermissionsState {
        val key = action.updatedFeature
        val value = state.getValue(key)

        return when (action) {
            is WebsitePermissionAction.TogglePermission -> {
                val toggleable = value as WebsitePermission.Toggleable
                val newWebsitePermission = toggleable.copy(
                    status = action.updatedStatus,
                    isEnabled = action.updatedEnabledStatus
                )

                state + Pair(key, newWebsitePermission)
            }
            is WebsitePermissionAction.ChangeAutoplay -> {
                val autoplay = value as WebsitePermission.Autoplay
                val newWebsitePermission = autoplay.copy(
                    autoplayValue = action.autoplayValue
                )
                state + Pair(key, newWebsitePermission)
            }
        }
    }
}

object TrackingProtectionStateReducer {
    /**
     * Handles creating a new [TrackingProtectionState] based on the specific
     * [TrackingProtectionAction].
     */
    fun reduce(
        state: TrackingProtectionState,
        action: TrackingProtectionAction
    ): TrackingProtectionState {
        return when (action) {
            is TrackingProtectionAction.ToggleTrackingProtectionEnabled ->
                state.copy(isTrackingProtectionEnabled = action.isTrackingProtectionEnabled)
        }
    }
}
