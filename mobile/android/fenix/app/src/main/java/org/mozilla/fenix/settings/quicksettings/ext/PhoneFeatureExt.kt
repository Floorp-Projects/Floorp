/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.ext

import android.content.Context
import mozilla.components.concept.engine.permission.SitePermissions
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.utils.Settings

/**
 * Common [PhoneFeature] extensions used for **quicksettings**.
 *
 * Whether the website permission associated with this [PhoneFeature] should be shown to the user.
 */
fun PhoneFeature.shouldBeVisible(
    sitePermissions: SitePermissions?,
    settings: Settings
): Boolean {
    // We have to check if the site have a site permission exception,
    // if it doesn't the feature shouldn't be visible
    return if (sitePermissions == null) {
        false
    } else {
        getStatus(sitePermissions, settings) != SitePermissions.Status.NO_DECISION
    }
}

/**
 * Common [PhoneFeature] extensions used for **quicksettings**.
 *
 * Whether the website permission associated with this [PhoneFeature] should allow user interaction.
 */
fun PhoneFeature.shouldBeEnabled(
    context: Context,
    sitePermissions: SitePermissions?,
    settings: Settings
) = isAndroidPermissionGranted(context) && isUserPermissionGranted(sitePermissions, settings)

/**
 * Common [PhoneFeature] extensions used for **quicksettings**.
 *
 * Whether the website permission associated with this [PhoneFeature] was specifically allowed by the user.
 *
 * To check whether the needed Android permission is also allowed [PhoneFeature#isAndroidPermissionGranted()]
 * can be used.
 */
fun PhoneFeature.isUserPermissionGranted(
    sitePermissions: SitePermissions?,
    settings: Settings
) = getStatus(sitePermissions, settings) == SitePermissions.Status.ALLOWED
