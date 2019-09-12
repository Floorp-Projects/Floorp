/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.content.blocking.Tracker
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
        val listTrackers: List<Tracker>,
        val listTrackersLoaded: List<Tracker>,
        val mode: TrackingProtectionState.Mode
    ) : TrackingProtectionAction()

    data class UrlChange(val url: String) : TrackingProtectionAction()
    data class TrackerListChange(val listTrackers: List<Tracker>) : TrackingProtectionAction()
    data class TrackerLoadedListChange(val listTrackersLoaded: List<Tracker>) :
        TrackingProtectionAction()

    data class TrackerBlockingChanged(val isTrackingProtectionEnabled: Boolean) :
        TrackingProtectionAction()

    object ExitDetailsMode : TrackingProtectionAction()
    data class EnterDetailsMode(
        val category: TrackingProtectionCategory,
        val categoryBlocked: Boolean
    ) :
        TrackingProtectionAction()
}

/**
 * The state for the Tracking Protection Panel
 * @property url Current URL to display
 * @property isTrackingProtectionEnabled Current status of tracking protection for this session (ie is an exception)
 * @property listTrackers List of currently blocked Trackers
 * @property listTrackersLoaded List of currently not blocked Trackers
 * @property mode Current Mode of TrackingProtection
 */
data class TrackingProtectionState(
    val session: Session?,
    val url: String,
    val isTrackingProtectionEnabled: Boolean,
    val listTrackers: List<Tracker>,
    val listTrackersLoaded: List<Tracker>,
    val mode: Mode
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
enum class TrackingProtectionCategory(val title: Int, val description: Int) {
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
    FINGERPRINTERS(R.string.etp_fingerprinters_title, R.string.etp_fingerprinters_description),
    TRACKING_CONTENT(R.string.etp_tracking_content_title, R.string.etp_tracking_content_description)
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
        is TrackingProtectionAction.TrackerListChange -> state.copy(
            listTrackers = action.listTrackers
        )
        is TrackingProtectionAction.TrackerLoadedListChange -> state.copy(
            listTrackersLoaded = action.listTrackersLoaded
        )
        TrackingProtectionAction.ExitDetailsMode -> state.copy(
            mode = TrackingProtectionState.Mode.Normal
        )
        is TrackingProtectionAction.EnterDetailsMode -> state.copy(
            mode = TrackingProtectionState.Mode.Details(
                action.category,
                action.categoryBlocked
            )
        )
        is TrackingProtectionAction.TrackerBlockingChanged ->
            state.copy(isTrackingProtectionEnabled = action.isTrackingProtectionEnabled)
    }
}
