/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import androidx.annotation.StringRes
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.R

/**
 * The [Store] for holding the [TrackingProtectionState] and applying [TrackingProtectionAction]s.
 */
class TrackingProtectionStore(initialState: TrackingProtectionState) :
    Store<TrackingProtectionState, TrackingProtectionAction>(
        initialState,
        ::trackingProtectionStateReducer
    )

/**
 * Actions to dispatch through the `TrackingProtectionStore` to modify `TrackingProtectionState` through the reducer.
 */
sealed class TrackingProtectionAction : Action {
    data class Change(
        val url: String,
        val isTrackingProtectionEnabled: Boolean,
        val listTrackers: List<TrackerLog>,
        val mode: TrackingProtectionState.Mode
    ) : TrackingProtectionAction()

    data class UrlChange(val url: String) : TrackingProtectionAction()
    data class TrackerLogChange(val listTrackers: List<TrackerLog>) : TrackingProtectionAction()

    object ExitDetailsMode : TrackingProtectionAction()
    data class EnterDetailsMode(
        val category: TrackingProtectionCategory,
        val categoryBlocked: Boolean
    ) :
        TrackingProtectionAction()
}

/**
 * The state for the Tracking Protection Panel
 * @property tab Current session to display
 * @property url Current URL to display
 * @property isTrackingProtectionEnabled Current status of tracking protection for this session
 * (ie is an exception)
 * @property listTrackers Current Tracker Log list of blocked and loaded tracker categories
 * @property mode Current Mode of TrackingProtection
 * @property lastAccessedCategory Remembers the last accessed details category, used to move
 * accessibly focus after returning from details_mode
 */
data class TrackingProtectionState(
    val tab: SessionState?,
    val url: String,
    val isTrackingProtectionEnabled: Boolean,
    val listTrackers: List<TrackerLog>,
    val mode: Mode,
    val lastAccessedCategory: String
) : State {
    sealed class Mode {
        object Normal : Mode()
        data class Details(
            val selectedCategory: TrackingProtectionCategory,
            val categoryBlocked: Boolean
        ) : Mode()
    }
}

/**
 * The 5 categories of Tracking Protection to display
 */
enum class TrackingProtectionCategory(
    @StringRes val title: Int,
    @StringRes val description: Int
) {
    SOCIAL_MEDIA_TRACKERS(
        R.string.etp_social_media_trackers_title,
        R.string.etp_social_media_trackers_description
    ),
    CROSS_SITE_TRACKING_COOKIES(
        R.string.etp_cookies_title,
        R.string.etp_cookies_description
    ),
    CRYPTOMINERS(
        R.string.etp_cryptominers_title,
        R.string.etp_cryptominers_description
    ),
    FINGERPRINTERS(
        R.string.etp_fingerprinters_title,
        R.string.etp_fingerprinters_description
    ),
    TRACKING_CONTENT(
        R.string.etp_tracking_content_title,
        R.string.etp_tracking_content_description
    ),
    REDIRECT_TRACKERS(
        R.string.etp_redirect_trackers_title,
        R.string.etp_redirect_trackers_description
    )
}

/**
 * The TrackingProtectionState Reducer.
 */
fun trackingProtectionStateReducer(
    state: TrackingProtectionState,
    action: TrackingProtectionAction
): TrackingProtectionState {
    return when (action) {
        is TrackingProtectionAction.Change -> state.copy(
            url = action.url,
            isTrackingProtectionEnabled = action.isTrackingProtectionEnabled,
            listTrackers = action.listTrackers,
            mode = action.mode
        )
        is TrackingProtectionAction.UrlChange -> state.copy(
            url = action.url
        )
        is TrackingProtectionAction.TrackerLogChange -> state.copy(listTrackers = action.listTrackers)
        TrackingProtectionAction.ExitDetailsMode -> state.copy(
            mode = TrackingProtectionState.Mode.Normal
        )
        is TrackingProtectionAction.EnterDetailsMode -> state.copy(
            mode = TrackingProtectionState.Mode.Details(
                action.category,
                action.categoryBlocked
            ),
            lastAccessedCategory = action.category.name
        )
    }
}
