/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import mozilla.components.lib.state.State
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.PhoneFeature

/**
 * [State] containing all data displayed to the user by this Fragment.
 *
 * Partitioned further to contain mutiple states for each standalone View this Fragment holds.
 */
data class QuickSettingsFragmentState(
    val webInfoState: WebsiteInfoState,
    val websitePermissionsState: WebsitePermissionsState
) : State

/**
 * [State] to be rendered by [WebsiteInfoView] indicating whether the connection is secure or not.
 *
 * @param websiteUrl [String] the URL of the current web page.
 * @param websiteTitle [String] the title of the current web page.
 * @param websiteSecurityUiValues UI values to represent the security of the website.
 */
data class WebsiteInfoState(
    val websiteUrl: String,
    val websiteTitle: String,
    val websiteSecurityUiValues: WebsiteSecurityUiValues,
    val certificateName: String
) : State

enum class WebsiteSecurityUiValues(
    @StringRes val securityInfoRes: Int,
    @DrawableRes val iconRes: Int,
    @ColorRes val iconTintRes: Int
) {
    SECURE(
        R.string.quick_settings_sheet_secure_connection,
        R.drawable.mozac_ic_lock,
        R.color.photonGreen50
    ),
    INSECURE(
        R.string.quick_settings_sheet_insecure_connection,
        R.drawable.mozac_ic_globe,
        R.color.photonRed50
    )
}

/**
 * [State] to be rendered by [WebsitePermissionsView] displaying all explicitly allowed or blocked
 * website permissions.
 */
typealias WebsitePermissionsState = Map<PhoneFeature, WebsitePermission>

/**
 * Wrapper over a website permission encompassing all it's needed state to be rendered on the screen.
 *
 * Contains a limited number of implementations because there is a known, finite number of permissions
 * we need to display to the user.
 *
 * @property status The *allowed* / *blocked* permission status to be shown to the user.
 * @property isVisible Whether this permission should be shown to the user.
 * @property isEnabled Visual indication about whether this permission is *enabled* / *disabled*
 * @property isBlockedByAndroid Whether the corresponding *dangerous* Android permission is granted
 * for the app by the user or not.
 */
data class WebsitePermission(
    val phoneFeature: PhoneFeature,
    val status: String,
    val isVisible: Boolean,
    val isEnabled: Boolean,
    val isBlockedByAndroid: Boolean
)
