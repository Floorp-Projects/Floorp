/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.os.Parcelable
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import kotlinx.parcelize.Parcelize
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
        val cookieBannerUIMode: CookieBannerUIMode,
        val listTrackers: List<TrackerLog>,
        val mode: ProtectionsState.Mode,
    ) : ProtectionsAction()

    /**
     * Toggles the enabled state of cookie banner handling protection.
     *
     * @property cookieBannerUIMode the current status of the cookie banner handling mode.
     */
    data class ToggleCookieBannerHandlingProtectionEnabled(val cookieBannerUIMode: CookieBannerUIMode) :
        ProtectionsAction()

    /**
     * Reports a site domain where cookie banner reducer didn't work.
     *
     * @property url to report.
     */
    data class RequestReportSiteDomain(
        val url: String,
    ) : ProtectionsAction()

    /**
     * Indicates that cookie banner handling mode has been updated.
     */
    data class UpdateCookieBannerMode(val cookieBannerUIMode: CookieBannerUIMode) :
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
 * @property cookieBannerUIMode Current status of cookie banner handling protection
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
    val cookieBannerUIMode: CookieBannerUIMode,
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
 * CookieBannerUIMode - contains a description and icon that will be shown
 * in the protection view.
 */
@Parcelize
enum class CookieBannerUIMode(
    @StringRes val description: Int? = null,
    @DrawableRes val icon: Int? = null,
) : Parcelable {

    /**
     * ENABLE - The site domain wasn't added to the list of exceptions.
     */
    ENABLE(
        R.string.reduce_cookie_banner_on_for_site,
        R.drawable.ic_cookies_enabled,
    ),

    /**
     * DISABLE - The site domain was added to the exceptions list.
     * The cookie banner reducer will not work.
     */
    DISABLE(
        R.string.reduce_cookie_banner_off_for_site,
        R.drawable.ic_cookies_disabled,
    ),

    /**
     * SITE_NOT_SUPPORTED - The domain is not supported by cookie banner handling.
     */
    SITE_NOT_SUPPORTED(
        R.string.reduce_cookie_banner_unsupported_site,
        R.drawable.ic_cookies_disabled,
    ),

    /**
     * REQUEST_UNSUPPORTED_SITE_SUBMITTED - The user submitted a request
     * for adding support for cookie banner handling for the domain.
     */
    REQUEST_UNSUPPORTED_SITE_SUBMITTED(
        R.string.reduce_cookie_banner_unsupported_site_request_submitted_2,
        R.drawable.ic_cookies_disabled,
    ),

    /**
     HIDE - All the cookie banner handling in the tracking panel is hidden.
     */
    HIDE,
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
            cookieBannerUIMode = action.cookieBannerUIMode,
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
            cookieBannerUIMode = action.cookieBannerUIMode,
        )
        is ProtectionsAction.RequestReportSiteDomain -> state.copy(
            url = action.url,
        )
        is ProtectionsAction.UpdateCookieBannerMode -> state.copy(
            cookieBannerUIMode = action.cookieBannerUIMode,
        )
    }
}
