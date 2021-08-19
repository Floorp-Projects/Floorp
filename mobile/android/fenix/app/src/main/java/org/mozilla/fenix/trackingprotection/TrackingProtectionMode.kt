/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.os.Parcelable
import androidx.annotation.StringRes
import kotlinx.parcelize.Parcelize
import org.mozilla.fenix.R

@Parcelize
enum class TrackingProtectionMode(
    @StringRes val preferenceKey: Int,
    @StringRes val titleRes: Int,
    @StringRes val contentDescriptionRes: Int
) : Parcelable {

    STANDARD(
        preferenceKey = R.string.pref_key_tracking_protection_standard_option,
        titleRes = R.string.preference_enhanced_tracking_protection_standard_default_1,
        contentDescriptionRes = R.string.preference_enhanced_tracking_protection_standard_info_button
    ),
    STRICT(
        preferenceKey = R.string.pref_key_tracking_protection_strict_default,
        titleRes = R.string.preference_enhanced_tracking_protection_strict,
        contentDescriptionRes = R.string.preference_enhanced_tracking_protection_strict_info_button
    ),
    CUSTOM(
        preferenceKey = R.string.pref_key_tracking_protection_custom_option,
        titleRes = R.string.preference_enhanced_tracking_protection_custom,
        contentDescriptionRes = R.string.preference_enhanced_tracking_protection_custom_info_button
    )
}
