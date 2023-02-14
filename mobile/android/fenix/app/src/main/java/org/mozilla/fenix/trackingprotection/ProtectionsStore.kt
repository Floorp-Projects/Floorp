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
 * The [Store] for holding the [ProtectionsState] and applying [ProtectionsAction]s.
 */
class ProtectionsStore(initialState: ProtectionsState) :
    Store<ProtectionsState, ProtectionsAction>(
        initialState,
        ::protectionsStateReducer,
    )

/**
 * Actions to dispatch through the `TrackingProtectionStore` to modify `ProtectionsState` through the reducer.
 */
sealed class ProtectionsAction : Action {
    /**
     * The values of the tracking protection view has been changed.
     */
    data class Change(
        val url: String,
        val isTrackingProtectionEnabled: Boolean,
        val isCookieBannerHandlingEnabled: Boolean,
        val listTrackers: List<TrackerLog>,
        val mode: ProtectionsState.Mode,
    ) : ProtectionsAction()

    /**
     * Toggles the enabled state of cookie banner handling protection.
     *
     * @param isEnabled Whether or not cookie banner protection is enabled.
     */
    data class ToggleCookieBannerHandlingProtectionEnabled(val isEnabled: Boolean) :
        ProtectionsAction()

    /**
     * Indicates the url has changed.
     */
    data class UrlChange(val url: String) : ProtectionsAction()

    /**
     * Indicates the url has the list of trackers has been updated.
     */
    data class TrackerLogChange(val listTrackers: List<TrackerLog>) : ProtectionsAction()

    /**
     * Indicates the user is leaving the detailed view.
     */
    object ExitDetailsMode : ProtectionsAction()

    /**
     * Holds the data to show a detailed tracking protection view.
     */
    data class EnterDetailsMode(
        val category: TrackingProtectionCategory,
        val categoryBlocked: Boolean,
    ) : ProtectionsAction()
}

/**
 * The state for the Protections Panel
 * @property tab Current session to display
 * @property url Current URL to display
 * @property isTrackingProtectionEnabled Current status of tracking protection for this session
 * (ie is an exception)
 * @property isCookieBannerHandlingEnabled Current status of cookie banner handling protection
 * for this session (ie is an exception).
 * @property listTrackers Current Tracker Log list of blocked and loaded tracker categories
 * @property mode Current Mode of TrackingProtection
 * @property lastAccessedCategory Remembers the last accessed details category, used to move
 * accessibly focus after returning from details_mode
 */
data class ProtectionsState(
    val tab: SessionState?,
    val url: String,
    val isTrackingProtectionEnabled: Boolean,
    val isCookieBannerHandlingEnabled: Boolean,
    val listTrackers: List<TrackerLog>,
    val mode: Mode,
    val lastAccessedCategory: String,
) : State {
    /**
     * Indicates the modes in which a tracking protection view could be in.
     */
    sealed class Mode {
        /**
         * Indicates that tracking protection view should not be in detail mode.
         */
        object Normal : Mode()

        /**
         * Indicates that tracking protection view in detailed mode.
         */
        data class Details(
            val selectedCategory: TrackingProtectionCategory,
            val categoryBlocked: Boolean,
        ) : Mode()
    }
}

/**
 * The 5 categories of Tracking Protection to display
 */
enum class TrackingProtectionCategory(
    @StringRes val title: Int,
    @StringRes val description: Int,
) {
    SOCIAL_MEDIA_TRACKERS(
        R.string.etp_social_media_trackers_title,
        R.string.etp_social_media_trackers_description,
    ),
    CROSS_SITE_TRACKING_COOKIES(
        R.string.etp_cookies_title,
        R.string.etp_cookies_description,
    ),
    CRYPTOMINERS(
        R.string.etp_cryptominers_title,
        R.string.etp_cryptominers_description,
    ),
    FINGERPRINTERS(
        R.string.etp_fingerprinters_title,
        R.string.etp_fingerprinters_description,
    ),
    TRACKING_CONTENT(
        R.string.etp_tracking_content_title,
        R.string.etp_tracking_content_description,
    ),
    REDIRECT_TRACKERS(
        R.string.etp_redirect_trackers_title,
        R.string.etp_redirect_trackers_description,
    ),
}

/**
 * The [ProtectionsState] reducer.
 */
fun protectionsStateReducer(
    state: ProtectionsState,
    action: ProtectionsAction,
): ProtectionsState {
    return when (action) {
        is ProtectionsAction.Change -> state.copy(
            url = action.url,
            isTrackingProtectionEnabled = action.isTrackingProtectionEnabled,
            isCookieBannerHandlingEnabled = action.isCookieBannerHandlingEnabled,
            listTrackers = action.listTrackers,
            mode = action.mode,
        )
        is ProtectionsAction.UrlChange -> state.copy(
            url = action.url,
        )
        is ProtectionsAction.TrackerLogChange -> state.copy(listTrackers = action.listTrackers)
        ProtectionsAction.ExitDetailsMode -> state.copy(
            mode = ProtectionsState.Mode.Normal,
        )
        is ProtectionsAction.EnterDetailsMode -> state.copy(
            mode = ProtectionsState.Mode.Details(
                action.category,
                action.categoryBlocked,
            ),
            lastAccessedCategory = action.category.name,
        )
        is ProtectionsAction.ToggleCookieBannerHandlingProtectionEnabled -> state.copy(
            isCookieBannerHandlingEnabled = action.isEnabled,
        )
    }
}
